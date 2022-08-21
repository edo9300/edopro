// Copyright (C) 2021-2022 Edoardo Lolletti

#import <UIKit/UIKit.h>
#import <CoreFoundation/CoreFoundation.h>
#include <irrlicht.h>
#include <SExposedVideoData.h>
#include <mutex>
#include "../bufferio.h"
#include "../game.h"
#include "porting_ios.h"

static std::mutex* queued_messages_mutex;
static std::deque<std::function<void()>>* events;

@interface ActionCallbackDelegate : UIViewController<UITextFieldDelegate> {
}
@end

@implementation ActionCallbackDelegate
- (void)textFieldDidEndEditing:(UITextField *)textField reason:(UITextFieldDidEndEditingReason)reason
{
	queued_messages_mutex->lock();
	events->emplace_back([text=BufferIO::DecodeUTF8({textField.text.UTF8String})](){
		auto device = ygo::mainGame->device;
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
			if(/*send_enter*/true) {
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
@end

@interface UiPickerDelegate : UIViewController<UIPickerViewDelegate, UIPickerViewDataSource> {
}
@end

@implementation UiPickerDelegate
{
	NSMutableArray* elements;
	NSInteger size;
	NSInteger selected;
}

- (void)setElements:(NSMutableArray*)elems elements_size:(NSInteger)elsize
{
	elements = elems;
	size = elsize;
	selected = -1;
}

- (NSInteger)getSelected
{
	return selected;
}

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView {
	return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component {
	return size;
}

- (NSString *)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component {
 	return row >= size ? @"error" : elements[row];
}

- (void)pickerView:(UIPickerView *)thePickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component {
 	selected = row;
}

@end

namespace porting {

const irr::video::SExposedVideoData* exposed_data = nullptr;

void showErrorDialog(epro::stringview context, epro::stringview message){
	NSString *nscontext = [NSString stringWithUTF8String:context.data()];
	NSString *nsmessage = [NSString stringWithUTF8String:message.data()];
	UIAlertController *alert = [UIAlertController alertControllerWithTitle:nscontext message:nsmessage preferredStyle:UIAlertControllerStyleAlert];
	UIAlertAction *ok = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
		exit(0);
	}];
	[alert addAction:ok];
	UIViewController* controller = (__bridge UIViewController*)exposed_data->OpenGLiOS.ViewController;
	[controller presentViewController:alert animated:YES completion:nil];
}

void showComboBox(const std::vector<std::string>& parameters, int selected) {
	NSMutableArray* objc_parameters = [NSMutableArray new];
	for(const auto& param : parameters)
		[objc_parameters addObject: [NSString stringWithUTF8String:param.data()]];
	UiPickerDelegate* delegate = [[UiPickerDelegate alloc] init];
	[delegate setElements:objc_parameters elements_size:parameters.size()];
	UIPickerView * picker = [UIPickerView new];
	picker.delegate = delegate;
	picker.dataSource = delegate;
	picker.showsSelectionIndicator = YES;
	picker.frame = CGRectMake(5, 20, 250, 140);
	UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"" message:@"\n\n\n\n\n\n" preferredStyle:UIAlertControllerStyleAlert];
	[alert.view addSubview:picker];
	[alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleDefault handler:nil]];
	[alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
		int index = (int)[ delegate getSelected ];
		if(index == -1)
			return;
		queued_messages_mutex->lock();
		events->emplace_back([index](){
			auto device = ygo::mainGame->device;
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
	}]];
	[picker selectRow:selected inComponent:0 animated:true];
	UIViewController* controller = (__bridge UIViewController*)exposed_data->OpenGLiOS.ViewController;
	[controller presentViewController:alert animated:YES completion:nil];
}

static void EPRO_IOS_ShowTextInputWindow(epro::stringview curtext) {
	UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Text input" message:@"" preferredStyle:UIAlertControllerStyleAlert];
	[alert addAction:[UIAlertAction actionWithTitle:@"Done" style:UIAlertActionStyleDefault handler:nil]];
	[alert addTextFieldWithConfigurationHandler:^(UITextField *textField) {
		textField.placeholder = @"Enter text:";
		textField.text = [NSString stringWithUTF8String:curtext.data()];
		textField.delegate = [[ActionCallbackDelegate alloc] init];
	}];
	UIViewController* controller = (__bridge UIViewController*)exposed_data->OpenGLiOS.ViewController;
	[controller presentViewController:alert animated:YES completion:nil];
}

epro::path_string getWorkDir() {
	NSFileManager *filemgr;
	NSArray *dirPaths;
	NSString *docsDir;
	BOOL isDir;
	
	filemgr =[NSFileManager defaultManager];
	
	dirPaths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	
	docsDir = [dirPaths objectAtIndex:0];
	
	if ([filemgr fileExistsAtPath: docsDir isDirectory:&isDir] == NO)
	{
		NSError* error;
		[filemgr createDirectoryAtPath:docsDir withIntermediateDirectories:YES attributes:nil error:&error];
	}
	
	epro::path_string res = [docsDir UTF8String];
	
	[filemgr release];
	printf("%s\n", res.data());
	return res;
}

int changeWorkDir(const char* newdir) {
	return [[NSFileManager defaultManager] changeCurrentDirectoryPath:[NSString stringWithUTF8String:newdir]] == true;
}


int transformEvent(const irr::SEvent& event, bool& stopPropagation) {
	static irr::core::position2di m_pointer = irr::core::position2di(0, 0);
	auto device = ygo::mainGame->device;
	switch(event.EventType) {
		case irr::EET_MOUSE_INPUT_EVENT: {
			if(event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN) {
				auto hovered = ygo::mainGame->env->getRootGUIElement()->getElementFromPoint({ event.MouseInput.X, event.MouseInput.Y });
				if(hovered && hovered->isEnabled()) {
					if(hovered->getType() == irr::gui::EGUIET_EDIT_BOX) {
						bool retval = hovered->OnEvent(event);
						if(retval)
							ygo::mainGame->env->setFocus(hovered);
						EPRO_IOS_ShowTextInputWindow(BufferIO::EncodeUTF8(((irr::gui::IGUIEditBox *)hovered)->getText()));
						stopPropagation = retval;
						return retval;
					}
				}
			}
			break;
		}
		case irr::EET_SYSTEM_EVENT: {
			stopPropagation = false;
			switch(event.ApplicationEvent.EventType) {
				case irr::EAET_WILL_PAUSE: {
					ygo::mainGame->SaveConfig();
					break;
				}
				default: break;
			}
			return true;
		}
		case irr::EET_TOUCH_INPUT_EVENT: {
			//printf("Got touch input\n");
			irr::SEvent translated;
			memset(&translated, 0, sizeof(irr::SEvent));
			translated.EventType = irr::EET_MOUSE_INPUT_EVENT;
			
			translated.MouseInput.X = event.TouchInput.X;
			translated.MouseInput.Y = event.TouchInput.Y;
			translated.MouseInput.Control = false;
			
			switch(event.TouchInput.touchedCount) {
				case 1: {
					/*printf("event type is: %d\n", event.TouchInput.Event);
					printf("event.TouchInput.X is: %d\n", event.TouchInput.X);
					printf("event.TouchInput.Y is: %d\n", event.TouchInput.Y);*/
					switch(event.TouchInput.Event) {
						case irr::ETIE_PRESSED_DOWN:
							m_pointer = irr::core::position2di(event.TouchInput.X, event.TouchInput.Y);
							translated.MouseInput.Event = irr::EMIE_LMOUSE_PRESSED_DOWN;
							translated.MouseInput.ButtonStates = irr::EMBSM_LEFT;
							irr::SEvent hoverEvent;
							hoverEvent.EventType = irr::EET_MOUSE_INPUT_EVENT;
							hoverEvent.MouseInput.Event = irr::EMIE_MOUSE_MOVED;
							hoverEvent.MouseInput.X = event.TouchInput.X;
							hoverEvent.MouseInput.Y = event.TouchInput.Y;
							device->postEventFromUser(hoverEvent);
							break;
						case irr::ETIE_MOVED:
							m_pointer = irr::core::position2di(event.TouchInput.X, event.TouchInput.Y);
							translated.MouseInput.Event = irr::EMIE_MOUSE_MOVED;
							translated.MouseInput.ButtonStates = irr::EMBSM_LEFT;
							break;
						case irr::ETIE_LEFT_UP:
							translated.MouseInput.Event = irr::EMIE_LMOUSE_LEFT_UP;
							translated.MouseInput.ButtonStates = 0;
							// we don't have a valid pointer element use last
							// known pointer pos
							translated.MouseInput.X = m_pointer.X;
							translated.MouseInput.Y = m_pointer.Y;
							break;
						default:
							stopPropagation = true;
							return true;
					}
					break;
				}
				case 2: {
					if(event.TouchInput.Event == irr::ETIE_PRESSED_DOWN) {
						translated.MouseInput.Event = irr::EMIE_RMOUSE_PRESSED_DOWN;
						translated.MouseInput.ButtonStates = irr::EMBSM_LEFT | irr::EMBSM_RIGHT;
						translated.MouseInput.X = m_pointer.X;
						translated.MouseInput.Y = m_pointer.Y;
						device->postEventFromUser(translated);
						
						translated.MouseInput.Event = irr::EMIE_RMOUSE_LEFT_UP;
						translated.MouseInput.ButtonStates = irr::EMBSM_LEFT;
						
						device->postEventFromUser(translated);
					}
					return true;
				}
				case 3: {
					if(event.TouchInput.Event == irr::ETIE_PRESSED_DOWN) {
						translated.EventType = irr::EET_KEY_INPUT_EVENT;
						translated.KeyInput.Control = true;
						translated.KeyInput.PressedDown = false;
						translated.KeyInput.Key = irr::KEY_KEY_O;
						device->postEventFromUser(translated);
					}
					return true;
				}
				default:
					return true;
			}
			
			bool retval = device->postEventFromUser(translated);
			
			if(event.TouchInput.Event == irr::ETIE_LEFT_UP) {
				m_pointer = irr::core::position2di(0, 0);
			}
			stopPropagation = retval;
			return true;
		}
		default: break;
	}
	return false;
}

void dispatchQueuedMessages() {
	auto& _events = *events;
	std::unique_lock<std::mutex> lock(*queued_messages_mutex);
	while(!_events.empty()) {
		const auto event = _events.front();
		_events.pop_front();
		lock.unlock();
		event();
		lock.lock();
	}
}

}

extern int epro_ios_main(int argc, char *argv[]);

void irrlicht_main(){
	std::mutex _queued_messages_mutex;
	queued_messages_mutex = &_queued_messages_mutex;
	std::deque<std::function<void()>> _events;
	events=&_events;
	char* a[]={};
	if(epro_ios_main(0,a) == EXIT_SUCCESS)
		exit(0);
}
