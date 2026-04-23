// Copyright (C) 2021-2026 Edoardo Lolletti

#import <UIKit/UIKit.h>
#import <CoreFoundation/CoreFoundation.h>
#include <irrlicht.h>
#include <SExposedVideoData.h>
#include "../epro_mutex.h"
#include <array>
#include <string>
#include "../bufferio.h"
#include "../game.h"
#include "porting_ios.h"

static epro::mutex* queued_messages_mutex;
static std::deque<std::function<void()>>* events;

static void sendComboBoxResult(int index) {
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
}

static void sendTextInputResult(NSString* nstext) {
	queued_messages_mutex->lock();
	events->emplace_back([text=BufferIO::DecodeUTF8(nstext.UTF8String)](){
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


@interface ActionCallbackDelegate : UIViewController<UITextFieldDelegate> {
}
@end

@implementation ActionCallbackDelegate
- (void)textFieldDidEndEditing:(UITextField *)textField
{
	sendTextInputResult(textField.text);
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

@interface UiPickerViewDelegate : NSObject<UIAlertViewDelegate>

@property (nonatomic, assign) UIPickerView* picker;

@end

@implementation UiPickerViewDelegate

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
	if (buttonIndex != 1)
		return;
	sendComboBoxResult([ _picker selectedRowInComponent:0 ]);
}

@end

@interface TextInputDelegate : NSObject<UIAlertViewDelegate>

@end

@implementation TextInputDelegate

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
	sendTextInputResult([alertView textFieldAtIndex:0].text);
}

@end

namespace porting {

const irr::video::SExposedVideoData* exposed_data = nullptr;
static auto alert_controller = NSClassFromString(@"UIAlertController");
static auto alert_action = NSClassFromString(@"UIAlertAction");
static auto uialert_view = NSClassFromString(@"UIAlertView");

void showErrorDialog(epro::stringview context, epro::stringview message){
	@autoreleasepool {
		NSString *nscontext = [NSString stringWithUTF8String:context.data()];
		NSString *nsmessage = [NSString stringWithUTF8String:message.data()];
		if (alert_controller != nil) {
			UIAlertController *alert = [alert_controller alertControllerWithTitle:nscontext message:nsmessage preferredStyle:UIAlertControllerStyleAlert];
			UIAlertAction *ok = [alert_action actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull) {
				exit(0);
			}];
			[alert addAction:ok];
			UIViewController* controller = (__bridge UIViewController*)exposed_data->OpenGLiOS.ViewController;
			[controller presentViewController:alert animated:YES completion:nil];
		} else {
			UIAlertView* alert = [[uialert_view alloc] initWithTitle:nscontext message:nsmessage delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil];
			[alert show];
			while([alert isVisible]) {
				[[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
			}
			exit(0);
		}
	}
}

void showComboBox(const std::vector<std::string>& parameters, int selected) {
	if(parameters.empty())
		return;
	@autoreleasepool {
		NSMutableArray* objc_parameters = [NSMutableArray new];
		for(const auto& param : parameters)
			[objc_parameters addObject: [NSString stringWithUTF8String:param.data()]];
		UiPickerDelegate* delegate = [[UiPickerDelegate alloc] init];
		[delegate setElements:objc_parameters elements_size:parameters.size()];
		UIPickerView * picker = [UIPickerView new];
		picker.delegate = delegate;
		picker.dataSource = delegate;
		picker.showsSelectionIndicator = YES;
		if (alert_controller != nil) {
			picker.frame = CGRectMake(5, 20, 250, 140);
			UIAlertController *alert = [alert_controller alertControllerWithTitle:@"" message:@"\n\n\n\n\n\n" preferredStyle:UIAlertControllerStyleAlert];
			[alert.view addSubview:picker];
			[alert addAction:[alert_action actionWithTitle:@"Cancel" style:UIAlertActionStyleDefault handler:nil]];
			[alert addAction:[alert_action actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull) {
				sendComboBoxResult([ delegate getSelected]);
			}]];
			[picker selectRow:selected inComponent:0 animated:true];
			UIViewController* controller = (__bridge UIViewController*)exposed_data->OpenGLiOS.ViewController;
			[controller presentViewController:alert animated:YES completion:nil];
		} else {
			auto* picker_view_delegate = [[UiPickerViewDelegate alloc] init];
			picker_view_delegate.picker = picker;
			picker.frame = CGRectMake(10, 5, 263, 80);
			UIAlertView* alert = [[uialert_view alloc] initWithTitle:@"" message:@"\n\n\n\n\n\n\n" delegate:picker_view_delegate cancelButtonTitle:@"Cancel" otherButtonTitles:@"OK", nil];
			[picker selectRow:selected inComponent:0 animated:true];
			[alert addSubview:picker];
			[alert show];
		}
	}
}

void showTextInputWindow(epro::stringview curtext) {
	@autoreleasepool {
		if (alert_controller != nil) {
			UIAlertController *alert = [alert_controller alertControllerWithTitle:@"Text input" message:@"" preferredStyle:UIAlertControllerStyleAlert];
			[alert addAction:[alert_action actionWithTitle:@"Done" style:UIAlertActionStyleDefault handler:nil]];
			[alert addTextFieldWithConfigurationHandler:^(UITextField *textField) {
				textField.placeholder = @"Enter text:";
				textField.text = [NSString stringWithUTF8String:curtext.data()];
				textField.delegate = [[ActionCallbackDelegate alloc] init];
			}];
			UIViewController* controller = (__bridge UIViewController*)exposed_data->OpenGLiOS.ViewController;
			[controller presentViewController:alert animated:YES completion:nil];
		} else {
			UIAlertView *alert = [[uialert_view alloc] initWithTitle:@"Text Input" message:@"" delegate:[TextInputDelegate alloc] cancelButtonTitle:@"Done" otherButtonTitles:nil];
			alert.alertViewStyle = UIAlertViewStylePlainTextInput;
			[alert textFieldAtIndex:0].text = [NSString stringWithUTF8String:curtext.data()];
			[alert show];
		}
	}
}

epro::path_string getWorkDir() {
	@autoreleasepool {
		NSFileManager *filemgr;
		NSArray *dirPaths;
		NSString *docsDir;
		BOOL isDir;

		filemgr = [NSFileManager defaultManager];

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
}

int changeWorkDir(const char* newdir) {
	return [[NSFileManager defaultManager] changeCurrentDirectoryPath:[NSString stringWithUTF8String:newdir]] == true;
}

int transformEvent(const irr::SEvent& event, bool& stopPropagation) {
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
						showTextInputWindow(BufferIO::EncodeUTF8(((irr::gui::IGUIEditBox *)hovered)->getText()));
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
		default: break;
	}
	return false;
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

void irrlicht_main(){
	epro::mutex _queued_messages_mutex;
	queued_messages_mutex = &_queued_messages_mutex;
	std::deque<std::function<void()>> _events;
	events = &_events;

	const auto workdir = porting::getWorkDir() + "/";

	std::array<const char*, 3> args{ {"", "-C", workdir.data()} };

	if(edopro_main(args.size(), (char**)args.data()) == EXIT_SUCCESS)
		exit(0);
}
