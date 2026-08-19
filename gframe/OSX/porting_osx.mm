#include "porting_osx.h"

#include <AvailabilityMacros.h>
#import <AppKit/AppKit.h>
#include <unistd.h>

#include "../utils.h"

#if !defined(MAC_OS_X_VERSION_10_12) || MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12
#define NSEventModifierFlagControl NSControlKeyMask
#define NSEventModifierFlagCommand NSCommandKeyMask
#endif

static void (*fullscreenToggledCallback)() = NULL;

@interface EdoproHandler : NSObject
-(void)spawn;
-(void)toggle;
@end

@implementation EdoproHandler
-(void)spawn {
	const char* abspath = [[[NSBundle mainBundle] bundlePath] UTF8String];
	const auto& workdir = ygo::Utils::GetWorkingDirectory();
	const auto* workdir_cstr = workdir.data();
	auto pid = vfork();
	if(pid == 0) {
		execlp("open", "open", "-n", abspath, "--args", "-C", workdir_cstr, "-m", nullptr);
		_exit(EXIT_FAILURE);
	}
	if(pid < 0 || waitpid(pid, nullptr, WNOHANG) != 0)
		return;
}

-(void)toggle {
	[NSApp activateIgnoringOtherApps:YES];
#if defined(__MAC_10_7) && defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7
	[[NSApp mainWindow] toggleFullScreen:nil];
#endif
	fullscreenToggledCallback();
}
@end

static EdoproHandler* handler;

namespace porting {

void setupMenuBar(void (*fullscreenCallback)(void)) {
	fullscreenToggledCallback = fullscreenCallback;
	@autoreleasepool {
		// Apparently in a newer version of Irrlicht's CIrrDeviceOSX.mm

		NSString* bundleName = @""; // Cannot actually set the main menu title at runtime
		NSMenu* systemMenuBar = [[[NSMenu alloc] initWithTitle:@"MainMenu"] autorelease];
		NSMenu* appMainMenu = [[[NSMenu alloc] initWithTitle:bundleName] autorelease];
		NSMenu* dockMenu = [[NSApp delegate] applicationDockMenu:NSApp];
		handler = [[EdoproHandler alloc] init];

		NSMenuItem* appMainMenuOpener = [systemMenuBar addItemWithTitle:bundleName action:nil keyEquivalent:@""];
		[systemMenuBar setSubmenu:appMainMenu forItem:appMainMenuOpener];

		NSMenuItem* newWindowItem = [appMainMenu addItemWithTitle:@"New Window" action:@selector(spawn) keyEquivalent:@"n"];
		[newWindowItem setTarget:handler];
		[newWindowItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
		[dockMenu addItem:[newWindowItem copyWithZone:nil]];

		NSMenuItem* fullScreenItem = [appMainMenu addItemWithTitle:@"Toggle Full Screen" action:@selector(toggle) keyEquivalent:@"f"];
		[fullScreenItem setTarget:handler];
		[fullScreenItem setKeyEquivalentModifierMask:NSEventModifierFlagControl+NSEventModifierFlagCommand];
		[dockMenu addItem:[fullScreenItem copyWithZone:nil]];

#if defined(__MAC_10_7) && defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7
		NSWindowCollectionBehavior newBehavior = [[NSApp mainWindow] collectionBehavior];
		newBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
		[[NSApp mainWindow] setCollectionBehavior:newBehavior];
#endif

		NSMenuItem* quitItem = [appMainMenu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
		[quitItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];

		[NSApp setMainMenu:systemMenuBar];
	}
}

void toggleFullScreen() {
	[handler toggle];
}

std::string getWindowRect(void* _window) {
	NSWindow* Window = (NSWindow*)_window;
	NSString* str = NSStringFromRect(Window.frame);
	return [str UTF8String];
}

void setWindowRect(void* _window, const char* rect_string) {
	NSWindow* Window = (NSWindow*)_window;
	NSString* str = [NSString stringWithUTF8String : rect_string];
	NSRect frame = NSRectFromString(str);
	if(frame.size.width && frame.size.height)
		[Window setFrame : frame display : YES];
}

}
