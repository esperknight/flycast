//
//  osx-main.cpp
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//
#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>
#include <OpenGL/gl3.h>
#include <sys/stat.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>

#include "types.h"
#include "hw/maple/maple_cfg.h"
#include "hw/maple/maple_devs.h"
#include "log/LogManager.h"
#include "rend/gui.h"
#include "osx_keyboard.h"
#include "osx_gamepad.h"
#include "emulator-osx-Bridging-Header.h"
#if defined(USE_SDL)
#include "sdl/sdl.h"
#endif
#include "stdclass.h"
#include "wsi/context.h"
#include "emulator.h"
#include "hw/pvr/Renderer_if.h"
#include "rend/mainui.h"

OSXKeyboardDevice keyboard(0);
static std::shared_ptr<OSXKbGamepadDevice> kb_gamepad(0);
static std::shared_ptr<OSXMouseGamepadDevice> mouse_gamepad(0);
unsigned int *pmo_buttons;
float *pmo_wheel_delta;
static UInt32 keyboardModifiers;

int darw_printf(const char* text, ...)
{
    va_list args;

    char temp[2048];
    va_start(args, text);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);

    NSLog(@"%s", temp);

    return 0;
}

void os_SetWindowText(const char * text) {
    puts(text);
}

void os_DoEvents() {
}

void UpdateInputState() {
#if defined(USE_SDL)
	input_sdl_handle();
#endif
}

void os_CreateWindow() {
#ifdef DEBUG
    int ret = task_set_exception_ports(
                                       mach_task_self(),
                                       EXC_MASK_BAD_ACCESS,
                                       MACH_PORT_NULL,
                                       EXCEPTION_DEFAULT,
                                       0);
    
    if (ret != KERN_SUCCESS) {
        printf("task_set_exception_ports: %s\n", mach_error_string(ret));
    }
#endif
}

void os_SetupInput()
{
#if defined(USE_SDL)
	input_sdl_init();
#endif

	kb_gamepad = std::make_shared<OSXKbGamepadDevice>(0);
	GamepadDevice::Register(kb_gamepad);
	mouse_gamepad = std::make_shared<OSXMouseGamepadDevice>(0);
	GamepadDevice::Register(mouse_gamepad);
}

void common_linux_setup();

void emu_dc_exit()
{
    dc_exit();
}

void emu_dc_term()
{
	if (dc_is_running())
		dc_exit();
	dc_term();
	LogManager::Shutdown();
}

void emu_gui_open_settings()
{
	gui_open_settings();
}

void emu_dc_resume()
{
	dc_resume();
}

extern int screen_width,screen_height;
extern bool rend_framePending();

bool emu_frame_pending()
{
	return rend_framePending() || gui_is_open();
}

bool emu_renderer_enabled()
{
	return mainui_loop_enabled();
}

int emu_single_frame(int w, int h)
{
    if (!emu_frame_pending())
        return 0;

    screen_width = w;
    screen_height = h;
    return (int)mainui_rend_frame();
}

void emu_gles_init(int width, int height)
{
	// work around https://bugs.swift.org/browse/SR-12263
	pmo_buttons = mo_buttons;
	pmo_wheel_delta = mo_wheel_delta;

    char *home = getenv("HOME");
    if (home != NULL)
    {
        std::string config_dir = std::string(home) + "/.reicast/";
        if (!file_exists(config_dir))
        	config_dir = std::string(home) + "/.flycast/";
        int instanceNumber = (int)[[NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.reicast.Flycast"] count];
        if (instanceNumber > 1){
            config_dir += std::to_string(instanceNumber) + "/";
            [[NSApp dockTile] setBadgeLabel:@(instanceNumber).stringValue];
        }
        mkdir(config_dir.c_str(), 0755); // create the directory if missing
        set_user_config_dir(config_dir);
        add_system_data_dir(config_dir);
        config_dir += "data/";
        mkdir(config_dir.c_str(), 0755);
        set_user_data_dir(config_dir);
    }
    else
    {
        set_user_config_dir("./");
        set_user_data_dir("./");
    }
    // Add bundle resources path
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
        add_system_data_dir(std::string(path) + "/");
    CFRelease(resourcesURL);
    CFRelease(mainBundle);

	// Calculate screen DPI
	NSScreen *screen = [NSScreen mainScreen];
	NSDictionary *description = [screen deviceDescription];
    CGDirectDisplayID displayID = [[description objectForKey:@"NSScreenNumber"] unsignedIntValue];
	CGSize displayPhysicalSize = CGDisplayScreenSize(displayID);
    
    //Neither CGDisplayScreenSize(description's NSScreenNumber) nor [NSScreen backingScaleFactor] can calculate the correct dpi in macOS. E.g. backingScaleFactor is always 2 in all display modes for rMBP 16"
    NSSize displayNativeSize;
    CFArrayRef allDisplayModes = CGDisplayCopyAllDisplayModes(displayID, NULL);
    CFIndex n = CFArrayGetCount(allDisplayModes);
    for(CFIndex i = 0; i < n; ++i)
    {
        CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex(allDisplayModes, i);
        if(CGDisplayModeGetIOFlags(m) & kDisplayModeNativeFlag)
        {
            displayNativeSize.width = CGDisplayModeGetPixelWidth(m);
            displayNativeSize.height = CGDisplayModeGetPixelHeight(m);
            break;
        }
    }
    CFRelease(allDisplayModes);
    
	screen_dpi = (int)(displayNativeSize.width / displayPhysicalSize.width * 25.4f);
    NSSize displayResolution;
    displayResolution.width = CGDisplayPixelsWide(displayID);
    displayResolution.height = CGDisplayPixelsHigh(displayID);
    scaling = displayNativeSize.width / displayResolution.width;
    
	screen_width = width;
	screen_height = height;

	InitRenderApi();
	mainui_init();
	mainui_enabled = true;
}

int emu_reicast_init()
{
	LogManager::Init();
	common_linux_setup();
	NSArray *arguments = [[NSProcessInfo processInfo] arguments];
	unsigned long argc = [arguments count];
	char **argv = (char **)malloc(argc * sizeof(char*));
	int paramCount = 0;
	for (unsigned long i = 0; i < argc; i++)
	{
		const char *arg = [[arguments objectAtIndex:i] UTF8String];
		if (!strncmp(arg, "-psn_", 5))
			// ignore Process Serial Number argument on first launch
			continue;
		argv[paramCount++] = strdup(arg);
	}
	
	int rc = reicast_init(paramCount, argv);
	
	for (unsigned long i = 0; i < paramCount; i++)
		free(argv[i]);
	free(argv);
	
	return rc;
}

void emu_key_input(UInt16 keyCode, bool pressed, UInt modifierFlags) {
	if (keyCode != 0xFF)
		keyboard.keyboard_input(keyCode, pressed, 0);
	else
	{
		// Modifier keys
		UInt32 changes = keyboardModifiers ^ modifierFlags;
		if (changes & NSEventModifierFlagShift)
			keyboard.keyboard_input(kVK_Shift, modifierFlags & NSEventModifierFlagShift, 0);
		if (changes & NSEventModifierFlagControl)
			keyboard.keyboard_input(kVK_Control, modifierFlags & NSEventModifierFlagControl, 0);
		if (changes & NSEventModifierFlagOption)
			keyboard.keyboard_input(kVK_Option, modifierFlags & NSEventModifierFlagOption, 0);
		keyboardModifiers = modifierFlags;
	}
	if ((modifierFlags
		 & (NSEventModifierFlagShift | NSEventModifierFlagControl | NSEventModifierFlagOption | NSEventModifierFlagCommand)) == 0)
		kb_gamepad->gamepad_btn_input(keyCode, pressed);
}
void emu_character_input(const char *characters) {
	if (characters != NULL)
		while (*characters != '\0')
			keyboard.keyboard_character(*characters++);
}

void emu_mouse_buttons(int button, bool pressed)
{
	mouse_gamepad->gamepad_btn_input(button, pressed);
}

void emu_set_mouse_position(int x, int y, int width, int height)
{
	SetMousePosition(x, y, width, height);
}

std::string os_Locale(){
    return [[[NSLocale preferredLanguages] objectAtIndex:0] UTF8String];
}

std::string os_PrecomposedString(std::string string){
    return [[[NSString stringWithUTF8String:string.c_str()] precomposedStringWithCanonicalMapping] UTF8String];
}
