#import <UIKit/UIKit.h>
#import <CoreFoundation/CoreFoundation.h>
#include <irrlicht.h>
#include "porting_ios.h"

void EPRO_IOS_ShowErrorDialog(void* controller, const char* context, const char* message){
    NSString *nscontext = [NSString stringWithUTF8String:context];
    NSString *nsmessage = [NSString stringWithUTF8String:message];
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:nscontext message:nsmessage preferredStyle:UIAlertControllerStyleAlert];
    
    UIAlertAction *ok = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
        exit(0);
    }];
    [alert addAction:ok];
    UIViewController* aa = (__bridge UIViewController*)controller;
    [aa presentViewController:alert animated:YES completion:nil];
}

epro::path_string EPRO_IOS_GetWorkDir() {
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

int EPRO_IOS_ChangeWorkDir(const char* newdir) {
    return [[NSFileManager defaultManager] changeCurrentDirectoryPath:[NSString stringWithUTF8String:newdir]] == true;
}


int EPRO_IOS_transformEvent(const void* sevent, int* stopPropagation, void* irrdevice) {
    static irr::core::position2di m_pointer = irr::core::position2di(0, 0);
    const irr::SEvent& event = *(const irr::SEvent*)sevent;
    auto* device = (irr::IrrlichtDevice*)irrdevice;
    
    switch(event.EventType) {
            /*
             * partial implementation from https://github.com/minetest/minetest/blob/02a23892f94d3c83a6bdc301defc0e7ade7e1c2b/src/gui/modalMenu.cpp#L116
             * with this patch applied to the engine https://github.com/minetest/minetest/blob/02a23892f94d3c83a6bdc301defc0e7ade7e1c2b/build/android/patches/irrlicht-touchcount.patch
             */
        case irr::EET_TOUCH_INPUT_EVENT: {
            //printf("Got touch input\n");
            irr::SEvent translated;
            memset(&translated, 0, sizeof(irr::SEvent));
            translated.EventType = irr::EET_MOUSE_INPUT_EVENT;
            
            translated.MouseInput.X = event.TouchInput.X;
            translated.MouseInput.Y = event.TouchInput.Y;
            translated.MouseInput.Control = false;
            
            switch(1) {
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
                            *stopPropagation = 1;
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
                    if(event.TouchInput.Event == irr::ETIE_LEFT_UP) {
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
            *stopPropagation = retval;
            return true;
        }
        default: break;
    }
    return false;
}

extern int epro_ios_main(int argc, char *argv[]);

void irrlicht_main(){
    char* a[]={""};
    if(epro_ios_main(0,a) == EXIT_SUCCESS)
        exit(0);
}
