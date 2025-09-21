#ifndef __ANDROID__
#error This file may only be compiled for android!
#endif

#include "porting_android.h"

#include <android_native_app_glue.h>
#include <jni.h>
#include <irrlicht.h>
#include <vector>
#include <unistd.h>
#include <netinet/in.h>
#include "../log.h"
#include "../bufferio.h"
#include "../sound_manager.h"
#include "../game_config.h"
#include "../game.h"
#include "../epro_mutex.h"
#include "../epro_thread.h"
#include "../utils.h"

#define JPARAMS(...)  "(" __VA_ARGS__ ")"
#define JARRAY(...) "[" __VA_ARGS__
#define JSTRING "Ljava/lang/String;"
#define JINT "I"
#define JVOID "V"
#define JBYTE "B"
#define JBOOL "Z"

namespace ygo {

extern epro::thread::id main_thread_id;
epro::thread::id main_thread_id;

}

namespace {

std::string JstringtoCA(JNIEnv* env, const jstring& jnistring) {
	if(!jnistring)
		return "";

	const auto stringClass = env->GetObjectClass(jnistring);
	const auto getBytes = env->GetMethodID(stringClass, "getBytes", JPARAMS(JSTRING)JARRAY(JBYTE));
	jstring UTF8_STRING = env->NewStringUTF("UTF-8");
	// calls: byte[] stringJbytes = jnistring.getBytes("UTF-8")
	const auto stringJbytes = static_cast<jbyteArray>(env->CallObjectMethod(jnistring, getBytes, UTF8_STRING));

	size_t length = (size_t)env->GetArrayLength(stringJbytes);
	jbyte* pBytes = env->GetByteArrayElements(stringJbytes, nullptr);

	std::string ret{ (char*)pBytes, length };
	env->ReleaseByteArrayElements(stringJbytes, pBytes, JNI_ABORT);

	env->DeleteLocalRef(stringJbytes);
	env->DeleteLocalRef(stringClass);
	env->DeleteLocalRef(UTF8_STRING);
	return ret;
}

inline std::wstring JstringtoCW(JNIEnv* env, const jstring& jnistring) {
	return BufferIO::DecodeUTF8(JstringtoCA(env, jnistring));
}

jstring NewJavaString(JNIEnv* env, epro::stringview string) {
	jbyteArray jBuff = env->NewByteArray(string.size());
	env->SetByteArrayRegion(jBuff, 0, string.size(), (jbyte*)string.data());
	jclass cls_String = env->FindClass("java/lang/String");
	jmethodID String_new = env->GetMethodID(cls_String, "<init>", JPARAMS(JARRAY(JBYTE)JSTRING)JVOID);
	jstring UTF8_STRING = env->NewStringUTF("UTF-8");
	// calls: String ret = new String(jBuff, "UTF-8")
	jstring ret = static_cast<jstring>(env->NewObject(cls_String, String_new, jBuff, UTF8_STRING));

	env->DeleteLocalRef(jBuff);
	env->DeleteLocalRef(UTF8_STRING);
	env->DeleteLocalRef(cls_String);
	return ret;
}

epro::mutex* queued_messages_mutex = nullptr;
std::atomic_bool error_dialog_returned{ true };
std::deque<std::function<void()>>* events = nullptr;
std::unique_ptr<std::unique_lock<epro::mutex>> mainGameMutex = nullptr;
}

extern "C" {
	JNIEXPORT void JNICALL Java_io_github_edo9300_edopro_EpNativeActivity_putMessageBoxResult(
		JNIEnv* env, jclass thiz, jstring textString, jboolean send_enter) {
		if(porting::app_global->userData) {
			queued_messages_mutex->lock();
			events->emplace_back([send_enter, text = JstringtoCW(env, textString)](){
				auto device = static_cast<irr::IrrlichtDevice*>(porting::app_global->userData);
				auto irrenv = device->getGUIEnvironment();
				auto element = irrenv->getFocus();
				if(element && element->getType() == irr::gui::EGUIET_EDIT_BOX) {
					auto editbox = static_cast<irr::gui::IGUIEditBox*>(element);
					editbox->setText(text.data());
					irrenv->removeFocus(editbox);
					irrenv->setFocus(editbox->getParent());
					irr::SEvent changeEvent;
					changeEvent.EventType = irr::EET_GUI_EVENT;
					changeEvent.GUIEvent.Caller = editbox;
					changeEvent.GUIEvent.Element = 0;
					changeEvent.GUIEvent.EventType = irr::gui::EGET_EDITBOX_CHANGED;
					editbox->getParent()->OnEvent(changeEvent);
					if(send_enter) {
						irr::SEvent enterEvent;
						enterEvent.EventType = irr::EET_GUI_EVENT;
						enterEvent.GUIEvent.Caller = editbox;
						enterEvent.GUIEvent.Element = 0;
						enterEvent.GUIEvent.EventType = irr::gui::EGET_EDITBOX_ENTER;
						editbox->getParent()->OnEvent(enterEvent);
					}
				}
			});
			queued_messages_mutex->unlock();
		}
	}

	JNIEXPORT void JNICALL Java_io_github_edo9300_edopro_EpNativeActivity_putComboBoxResult(
		JNIEnv* env, jclass thiz, jint index) {
		if(porting::app_global->userData) {
			queued_messages_mutex->lock();
			events->emplace_back([index]() {
				auto device = static_cast<irr::IrrlichtDevice*>(porting::app_global->userData);
				auto irrenv = device->getGUIEnvironment();
				auto element = irrenv->getFocus();
				if(element && element->getType() == irr::gui::EGUIET_COMBO_BOX) {
					auto combobox = static_cast<irr::gui::IGUIComboBox*>(element);
					combobox->setSelected(index);
					irr::SEvent changeEvent;
					changeEvent.EventType = irr::EET_GUI_EVENT;
					changeEvent.GUIEvent.Caller = combobox;
					changeEvent.GUIEvent.Element = 0;
					changeEvent.GUIEvent.EventType = irr::gui::EGET_COMBO_BOX_CHANGED;
					combobox->getParent()->OnEvent(changeEvent);
				}
			});
			queued_messages_mutex->unlock();
		}
	}

	JNIEXPORT void JNICALL Java_io_github_edo9300_edopro_EpNativeActivity_errorDialogReturn(
		JNIEnv* env, jclass thiz) {
		error_dialog_returned = true;
	}
}

namespace porting {

std::string internal_storage = "";
std::string working_directory = "";

android_app* app_global = nullptr;
JNIEnv* jnienv = nullptr;
jclass       nativeActivity;

std::vector<std::string> GetExtraParameters() {
	std::vector<std::string> ret;
	ret.push_back("");//dummy arg 0

	jobject me = app_global->activity->clazz;

	jclass acl = jnienv->GetObjectClass(me); //class pointer of NativeActivity
	jmethodID giid = jnienv->GetMethodID(acl, "getIntent", JPARAMS()"Landroid/content/Intent;");
	jobject intent = jnienv->CallObjectMethod(me, giid); //Got our intent

	jnienv->DeleteLocalRef(acl);

	jclass icl = jnienv->GetObjectClass(intent); //class pointer of Intent
	jmethodID gseid = jnienv->GetMethodID(icl, "getStringArrayExtra", JPARAMS(JSTRING)JARRAY(JSTRING));
	jnienv->DeleteLocalRef(icl);

	auto argstring = jnienv->NewStringUTF("ARGUMENTS");

	jobjectArray stringArrays = (jobjectArray)jnienv->CallObjectMethod(intent, gseid, argstring);

	jnienv->DeleteLocalRef(argstring);
	jnienv->DeleteLocalRef(intent);

	int size = jnienv->GetArrayLength(stringArrays);

	for(int i = 0; i < size; ++i) {
		jstring string = (jstring)jnienv->GetObjectArrayElement(stringArrays, i);
		ret.push_back(JstringtoCA(jnienv, string));
		jnienv->DeleteLocalRef(string);
	}
	jnienv->DeleteLocalRef(stringArrays);
	return ret;
}

jclass findClass(std::string classname, JNIEnv* env = nullptr) {
	env = env ? env : jnienv;
	if(env == nullptr)
		return 0;

	jclass nativeactivity = env->FindClass("android/app/NativeActivity");
	jmethodID getClassLoader = env->GetMethodID(nativeactivity, "getClassLoader", JPARAMS()"Ljava/lang/ClassLoader;");
	jobject cls = env->CallObjectMethod(app_global->activity->clazz, getClassLoader);
	jclass classLoader = env->FindClass("java/lang/ClassLoader");
	jmethodID findClass = env->GetMethodID(classLoader, "loadClass", JPARAMS(JSTRING)"Ljava/lang/Class;");
	jstring strClassName = env->NewStringUTF(classname.c_str());
	auto ret = static_cast<jclass>(env->CallObjectMethod(cls, findClass, strClassName));

	jnienv->DeleteLocalRef(nativeactivity);
	jnienv->DeleteLocalRef(cls);
	jnienv->DeleteLocalRef(classLoader);
	jnienv->DeleteLocalRef(strClassName);

	return ret;
}

void initAndroid() {
	jnienv = nullptr;
	JavaVM* jvm = app_global->activity->vm;
	JavaVMAttachArgs lJavaVMAttachArgs;
	lJavaVMAttachArgs.version = JNI_VERSION_1_6;
	lJavaVMAttachArgs.name = "Edopro NativeThread";
	lJavaVMAttachArgs.group = nullptr;
	if(jvm->AttachCurrentThread(&jnienv, &lJavaVMAttachArgs) == JNI_ERR) {
		LOGE("Couldn't attach current thread");
		exit(-1);
	}
	auto localNativeActivity = findClass("io/github/edo9300/edopro/EpNativeActivity");
	if(!localNativeActivity) {
		LOGE("Couldn't retrieve nativeActivity");
		exit(-1);
	}
	nativeActivity = static_cast<jclass>(jnienv->NewGlobalRef(localNativeActivity));
	jnienv->DeleteLocalRef(localNativeActivity);
	ygo::main_thread_id = ygo::Utils::GetCurrThreadId();
}

void cleanupAndroid() {
	JavaVM* jvm = app_global->activity->vm;
	jnienv->DeleteGlobalRef(nativeActivity);
	jvm->DetachCurrentThread();
}

void displayKeyboard(bool pShow) {
	// Retrieves NativeActivity.
	jobject lNativeActivity = app_global->activity->clazz;
	jclass ClassNativeActivity = jnienv->GetObjectClass(lNativeActivity);

	// Retrieves Context.INPUT_METHOD_SERVICE.
	jclass ClassContext = jnienv->FindClass("android/content/Context");
	jfieldID FieldINPUT_METHOD_SERVICE =
		jnienv->GetStaticFieldID(ClassContext,
								 "INPUT_METHOD_SERVICE", "Ljava/lang/String;");
	jobject INPUT_METHOD_SERVICE =
		jnienv->GetStaticObjectField(ClassContext,
									 FieldINPUT_METHOD_SERVICE);

	jnienv->DeleteLocalRef(ClassContext);

	// Runs getSystemService(Context.INPUT_METHOD_SERVICE).
	jclass ClassInputMethodManager = jnienv->FindClass(
		"android/view/inputmethod/InputMethodManager");
	jmethodID MethodGetSystemService = jnienv->GetMethodID(
		ClassNativeActivity, "getSystemService", JPARAMS(JSTRING)"Ljava/lang/Object;");
	jobject lInputMethodManager = jnienv->CallObjectMethod(
		lNativeActivity, MethodGetSystemService,
		INPUT_METHOD_SERVICE);

	jnienv->DeleteLocalRef(INPUT_METHOD_SERVICE);

	// Runs getWindow().getDecorView().
	jmethodID MethodGetWindow = jnienv->GetMethodID(
		ClassNativeActivity, "getWindow", JPARAMS()"Landroid/view/Window;");
	jobject lWindow = jnienv->CallObjectMethod(lNativeActivity,
											   MethodGetWindow);
	jclass ClassWindow = jnienv->FindClass(
		"android/view/Window");
	jmethodID MethodGetDecorView = jnienv->GetMethodID(
		ClassWindow, "getDecorView", JPARAMS()"Landroid/view/View;");
	jobject lDecorView = jnienv->CallObjectMethod(lWindow,
												  MethodGetDecorView);

	jnienv->DeleteLocalRef(lWindow);
	jnienv->DeleteLocalRef(ClassWindow);

	jint lFlags = 0;
	if(pShow) {
		// Runs lInputMethodManager.showSoftInput(...).
		jmethodID MethodShowSoftInput = jnienv->GetMethodID(
			ClassInputMethodManager, "showSoftInput", JPARAMS("Landroid/view/View;" JINT)JBOOL);
		(void)jnienv->CallBooleanMethod(
			lInputMethodManager, MethodShowSoftInput,
			lDecorView, lFlags);
	} else {
		// Runs lWindow.getViewToken()
		jclass ClassView = jnienv->FindClass(
			"android/view/View");
		jmethodID MethodGetWindowToken = jnienv->GetMethodID(
			ClassView, "getWindowToken", JPARAMS()"Landroid/os/IBinder;");

		jnienv->DeleteLocalRef(ClassView);

		jobject lBinder = jnienv->CallObjectMethod(lDecorView,
												   MethodGetWindowToken);

		// lInputMethodManager.hideSoftInput(...).
		jmethodID MethodHideSoftInput = jnienv->GetMethodID(
			ClassInputMethodManager, "hideSoftInputFromWindow",
			JPARAMS("Landroid/os/IBinder;" JINT)JBOOL);
		(void)jnienv->CallBooleanMethod(
			lInputMethodManager, MethodHideSoftInput,
			lBinder, lFlags);

		jnienv->DeleteLocalRef(lBinder);
	}

	jnienv->DeleteLocalRef(ClassNativeActivity);
	jnienv->DeleteLocalRef(lInputMethodManager);
	jnienv->DeleteLocalRef(ClassInputMethodManager);
	jnienv->DeleteLocalRef(lDecorView);
}

void showInputDialog(epro::path_stringview current) {
	jmethodID showdialog = jnienv->GetMethodID(nativeActivity, "showDialog", JPARAMS(JSTRING)JVOID);

	if(showdialog == 0) {
		assert("porting::showInputDialog unable to find java show dialog method" == 0);
	}

	jstring jcurrent = NewJavaString(jnienv, current);

	jnienv->CallVoidMethod(app_global->activity->clazz, showdialog, jcurrent);

	jnienv->DeleteLocalRef(jcurrent);
}

void showComboBox(const std::vector<std::string>& parameters, int selected) {
	(void)selected;
	jmethodID showbox = jnienv->GetMethodID(nativeActivity, "showComboBox", JPARAMS(JARRAY(JSTRING))JVOID);

	jsize len = parameters.size();
	jobjectArray jlist = jnienv->NewObjectArray(len, jnienv->FindClass("java/lang/String"), 0);

	for(size_t i = 0; i < parameters.size(); i++) {
		auto jstring = NewJavaString(jnienv, parameters[i]);
		jnienv->SetObjectArrayElement(jlist, i, jstring);
		jnienv->DeleteLocalRef(jstring);
	}

	jnienv->CallVoidMethod(app_global->activity->clazz, showbox, jlist);

	jnienv->DeleteLocalRef(jlist);
}

bool transformEvent(const irr::SEvent& event, bool& stopPropagation) {
	switch(event.EventType) {
		case irr::EET_MOUSE_INPUT_EVENT: {
			if(event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN) {
				auto hovered = ygo::mainGame->env->getRootGUIElement()->getElementFromPoint({ event.MouseInput.X, event.MouseInput.Y });
				if(hovered && hovered->isEnabled()) {
					if(hovered->getType() == irr::gui::EGUIET_EDIT_BOX) {
						bool retval = hovered->OnEvent(event);
						if(retval)
							ygo::mainGame->env->setFocus(hovered);
						if(ygo::gGameConfig->native_keyboard) {
							porting::displayKeyboard(true);
						} else {
							porting::showInputDialog(BufferIO::EncodeUTF8(((irr::gui::IGUIEditBox*)hovered)->getText()));
						}
						stopPropagation = retval;
						return retval;
					}
				}
			}
			break;
		}
		case irr::EET_KEY_INPUT_EVENT: {
			if(ygo::gGameConfig->native_keyboard && event.KeyInput.Key == irr::KEY_RETURN) {
				porting::displayKeyboard(false);
			}
			break;
		}
		case irr::EET_SYSTEM_EVENT: {
			stopPropagation = false;
			switch(event.SystemEvent.AndroidCmd.Cmd) {
				case APP_CMD_PAUSE: {
					ygo::mainGame->SaveConfig();
					ygo::gSoundManager->PauseMusic(true);
					if(mainGameMutex == nullptr)
						mainGameMutex = std::unique_ptr<std::unique_lock<epro::mutex>>(new std::unique_lock<epro::mutex>(ygo::mainGame->gMutex));
					break;
				}
				case APP_CMD_STOP: {
					mainGameMutex = nullptr;
					break;
				}
				case APP_CMD_GAINED_FOCUS:
				case APP_CMD_LOST_FOCUS: {
					stopPropagation = true;
					break;
				}
				case APP_CMD_RESUME: {
					ygo::gSoundManager->PauseMusic(false);
					mainGameMutex = nullptr;
					break;
				}
				default: break;
			}
			return true;
		}
		default: break;
	}
	return false;
}

std::vector<epro::Address> getLocalIP() {
	std::vector<epro::Address> addresses;
	jmethodID getIP = jnienv->GetMethodID(nativeActivity, "getLocalIpAddresses", JPARAMS()JARRAY(JARRAY(JBYTE)));
	if(getIP == 0) {
		assert("porting::getLocalIP unable to find java getLocalIpAddresses method" == 0);
	}
	jobjectArray ipArray = (jobjectArray)jnienv->CallObjectMethod(app_global->activity->clazz, getIP);
	int size = jnienv->GetArrayLength(ipArray);

	for(int i = 0; i < size; ++i) {
		jbyteArray ipBuffer = static_cast<jbyteArray>(jnienv->GetObjectArrayElement(ipArray, i));
		int ipBufferSize = jnienv->GetArrayLength(ipBuffer);
		(void)ipBufferSize;
		jbyte* ipJava = jnienv->GetByteArrayElements(ipBuffer, nullptr);
		if(ipBufferSize == 4) {
			addresses.emplace_back(ipJava, epro::Address::INET);
		} else {
			addresses.emplace_back(ipJava, epro::Address::INET6);
		}
		jnienv->ReleaseByteArrayElements(ipBuffer, ipJava, JNI_ABORT);
		jnienv->DeleteLocalRef(ipBuffer);
	}

	jnienv->DeleteLocalRef(ipArray);
	return addresses;
}

#define JAVAVOIDSTRINGMETHOD(name)\
void name(epro::path_stringview arg) {\
jmethodID name = jnienv->GetMethodID(nativeActivity, #name, JPARAMS(JSTRING)JVOID);\
if(name == 0) assert("porting::" #name " unable to find java " #name " method" == 0);\
jstring jargs = NewJavaString(jnienv, arg);\
jnienv->CallVoidMethod(app_global->activity->clazz, name, jargs);\
jnienv->DeleteLocalRef(jargs);\
}

JAVAVOIDSTRINGMETHOD(launchWindbot)
JAVAVOIDSTRINGMETHOD(addWindbotDatabase)
JAVAVOIDSTRINGMETHOD(installUpdate)
JAVAVOIDSTRINGMETHOD(openUrl)
JAVAVOIDSTRINGMETHOD(openFile)
JAVAVOIDSTRINGMETHOD(shareFile)

void showErrorDialog(epro::stringview context, epro::stringview message) {
	jmethodID showDialog = jnienv->GetMethodID(nativeActivity, "showErrorDialog", JPARAMS(JSTRING JSTRING)JVOID);
	if(showDialog == 0)
		assert("porting::showErrorDialog unable to find java showErrorDialog method" == 0);
	jstring jcontext = NewJavaString(jnienv, context);
	jstring jmessage = NewJavaString(jnienv, message);

	error_dialog_returned = false;

	jnienv->CallVoidMethod(app_global->activity->clazz, showDialog, jcontext, jmessage);

	jnienv->DeleteLocalRef(jcontext);
	jnienv->DeleteLocalRef(jmessage);

	//keep parsing events so that the activity is drawn properly
	int Events = 0;
	android_poll_source* source = 0;
	while(!error_dialog_returned &&
		  ALooper_pollAll(-1, nullptr, &Events, (void**)&source) >= 0 &&
		  app_global->destroyRequested == 0) {
		if(source != NULL)
			source->process(app_global, source);
	}
}

void setTextToClipboard(epro::wstringview text) {
	jmethodID setClip = jnienv->GetMethodID(nativeActivity, "setClipboard", JPARAMS(JSTRING)JVOID);
	if(setClip == 0)
		assert("porting::setTextToClipboard unable to find java setClipboard method" == 0);
	jstring jargs = NewJavaString(jnienv, BufferIO::EncodeUTF8(text));

	jnienv->CallVoidMethod(app_global->activity->clazz, setClip, jargs);

	jnienv->DeleteLocalRef(jargs);
}

const wchar_t* getTextFromClipboard() {
	static std::wstring text;
	jmethodID getClip = jnienv->GetMethodID(nativeActivity, "getClipboard", JPARAMS()JSTRING);
	if(getClip == 0)
		assert("porting::getTextFromClipboard unable to find java getClipboard method" == 0);
	jstring js_clip = (jstring)jnienv->CallObjectMethod(app_global->activity->clazz, getClip);
	text = JstringtoCW(jnienv, js_clip);

	jnienv->DeleteLocalRef(js_clip);
	return text.c_str();
}

void dispatchQueuedMessages() {
	auto& _events = *events;
	std::unique_lock<epro::mutex> lock(*queued_messages_mutex);
	while(!_events.empty()) {
		const auto event = _events.front();
		_events.pop_front();
		lock.unlock();
		event();
		lock.lock();
	}
}

}

extern "C" int edopro_main(int argc, char* argv[]);

void android_main(android_app* app) {
	int retval = 0;
	porting::app_global = app;
	porting::initAndroid();
	porting::internal_storage = app->activity->internalDataPath;
	epro::mutex _queued_messages_mutex;
	queued_messages_mutex = &_queued_messages_mutex;
	std::deque<std::function<void()>> _events;
	events = &_events;

	auto strparams = porting::GetExtraParameters();
	std::vector<const char*> params;

	for(auto& param : strparams) {
		params.push_back(param.c_str());
	}
	//no longer needed after ndk 15c
	//app_dummy();
	retval = edopro_main(params.size(), (char**)params.data());

	porting::cleanupAndroid();
	_exit(retval);
}
