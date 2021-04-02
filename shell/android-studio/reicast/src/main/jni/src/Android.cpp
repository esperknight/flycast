#include <jni.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>
#include <unistd.h>
#include <stdlib.h>

#include "types.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <types.h>

#include "hw/maple/maple_cfg.h"
#include "profiler/profiler.h"
#include "rend/TexCache.h"
#include "rend/osd.h"
#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"
#include "hw/naomi/naomi_cart.h"
#include "oslib/audiostream.h"
#include "imgread/common.h"
#include "rend/gui.h"
#include "rend/osd.h"
#include "cfg/cfg.h"
#include "log/LogManager.h"
#include "wsi/context.h"
#include "emulator.h"
#include "rend/mainui.h"
#include "cfg/option.h"

JavaVM* g_jvm;

// Convenience class to get the java environment for the current thread.
// Also attach the threads, and detach it on destruction, if needed.
class JVMAttacher {
public:
    JVMAttacher() : _env(NULL), _detach_thread(false) {
    }
    JNIEnv *getEnv()
    {
        if (_env == NULL)
        {
            if (g_jvm == NULL) {
                die("g_jvm == NULL");
                return NULL;
            }
            int rc = g_jvm->GetEnv((void **)&_env, JNI_VERSION_1_6);
            if (rc  == JNI_EDETACHED) {
                if (g_jvm->AttachCurrentThread(&_env, NULL) != 0) {
                    die("AttachCurrentThread failed");
                    return NULL;
                }
                _detach_thread = true;
            }
            else if (rc == JNI_EVERSION) {
                die("JNI version error");
                return NULL;
            }
        }
        return _env;
    }

    ~JVMAttacher()
    {
        if (_detach_thread)
            g_jvm->DetachCurrentThread();
    }

private:
    JNIEnv *_env;
    bool _detach_thread;
};
static thread_local JVMAttacher jvm_attacher;

#include "android_gamepad.h"

#define SETTINGS_ACCESSORS(setting, type)                                                                                                    \
JNIEXPORT type JNICALL Java_com_reicast_emulator_emu_JNIdc_get ## setting(JNIEnv *env, jobject obj)  __attribute__((visibility("default")));           \
JNIEXPORT type JNICALL Java_com_reicast_emulator_emu_JNIdc_get ## setting(JNIEnv *env, jobject obj)                                                    \
{                                                                                                                                                       \
    return (type)config::setting;                                                                                                                           \
}

extern "C"
{
JNIEXPORT jstring JNICALL Java_com_reicast_emulator_emu_JNIdc_initEnvironment(JNIEnv *env, jobject obj, jobject emulator, jstring homeDirectory)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setExternalStorageDirectories(JNIEnv *env, jobject obj, jobjectArray pathList)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setGameUri(JNIEnv *env,jobject obj,jstring fileName)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_pause(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_resume(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_stop(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_destroy(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));

JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_send(JNIEnv *env,jobject obj,jint id, jint v)  __attribute__((visibility("default")));
JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_data(JNIEnv *env,jobject obj,jint id, jbyteArray d)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitNative(JNIEnv *env, jobject obj, jobject surface, jint w, jint h)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitJava(JNIEnv *env, jobject obj, jint w, jint h)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_rendframeJava(JNIEnv *env, jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendtermJava(JNIEnv *env, jobject obj)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vjoy(JNIEnv * env, jobject obj,int id,float x, float y, float w, float h)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_hideOsd(JNIEnv * env, jobject obj)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_getControllers(JNIEnv *env, jobject obj, jintArray controllers, jobjectArray peripherals)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)  __attribute__((visibility("default")));

SETTINGS_ACCESSORS(VirtualGamepadVibration, jint);
SETTINGS_ACCESSORS(AudioBufferSize, jint);

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_screenDpi(JNIEnv *env,jobject obj, jint screenDpi)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_guiOpenSettings(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsOpen(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsContentBrowser(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setButtons(JNIEnv *env, jobject obj, jbyteArray data) __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_init(JNIEnv *env, jobject obj) __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAdded(JNIEnv *env, jobject obj, jint id, jstring name, jint maple_port, jstring junique_id)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickRemoved(JNIEnv *env, jobject obj, jint id)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_virtualGamepadEvent(JNIEnv *env, jobject obj, jint kcode, jint joyx, jint joyy, jint lt, jint rt)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickButtonEvent(JNIEnv *env, jobject obj, jint id, jint key, jboolean pressed)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAxisEvent(JNIEnv *env, jobject obj, jint id, jint key, jint value) __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_mouseEvent(JNIEnv *env, jobject obj, jint xpos, jint ypos, jint buttons) __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_AudioBackend_setInstance(JNIEnv *env, jobject obj, jobject instance) __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_BaseGLActivity_register(JNIEnv *env, jobject obj, jobject activity) __attribute__((visibility("default")));
};

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_screenDpi(JNIEnv *env,jobject obj, jint screenDpi)
{
    screen_dpi = screenDpi;
}

extern int screen_width,screen_height;

float vjoy_pos[15][8];

extern bool print_stats;
static bool game_started;

//stuff for saving prefs
jobject g_emulator;
jmethodID saveAndroidSettingsMid;
static ANativeWindow *g_window = 0;

static void emuEventCallback(Event event)
{
	switch (event)
	{
	case Event::Pause:
		game_started = false;
		break;
	case Event::Resume:
		game_started = true;
		break;
	default:
		break;
	}
}

void os_DoEvents()
{
}

void os_CreateWindow()
{
}

void UpdateInputState()
{
}

void common_linux_setup();

void os_SetupInput()
{
}

void os_SetWindowText(char const *Text)
{
}

JNIEXPORT jstring JNICALL Java_com_reicast_emulator_emu_JNIdc_initEnvironment(JNIEnv *env, jobject obj, jobject emulator, jstring homeDirectory)
{
    // Initialize platform-specific stuff
    common_linux_setup();

    bool first_init = false;

    // Keep reference to global JVM and Emulator objects
    if (g_jvm == NULL)
    {
        first_init = true;
        env->GetJavaVM(&g_jvm);
    }
    if (g_emulator == NULL) {
        g_emulator = env->NewGlobalRef(emulator);
        saveAndroidSettingsMid = env->GetMethodID(env->GetObjectClass(emulator), "SaveAndroidSettings", "(Ljava/lang/String;)V");
    }
    // Set home directory based on User config
    if (homeDirectory != NULL)
    {
    	const char *jchar = env->GetStringUTFChars(homeDirectory, 0);
    	std::string path = jchar;
		if (!path.empty())
		{
			if (path.back() != '/')
				path += '/';
			set_user_config_dir(path);
			add_system_data_dir(path);
			std::string data_path = path + "data/";
			set_user_data_dir(data_path);
			if (!file_exists(data_path))
			{
				if (!make_directory(data_path))
				{
					WARN_LOG(BOOT, "Cannot create 'data' directory");
					set_user_data_dir(path);
				}
			}
		}
    	env->ReleaseStringUTFChars(homeDirectory, jchar);
    }
    INFO_LOG(BOOT, "Config dir is: %s", get_writable_config_path("").c_str());
    INFO_LOG(BOOT, "Data dir is:   %s", get_writable_data_path("").c_str());

    if (first_init)
    {
        // Do one-time initialization
    	LogManager::Init();
    	EventManager::listen(Event::Pause, emuEventCallback);
    	EventManager::listen(Event::Resume, emuEventCallback);
        jstring msg = NULL;
        int rc = reicast_init(0, NULL);
        if (rc == -4)
            msg = env->NewStringUTF("Cannot find configuration");
        else if (rc == 69)
            msg = env->NewStringUTF("Invalid command line");
        else if (rc == -1)
            msg = env->NewStringUTF("Memory initialization failed");
        return msg;
    }
    else
        return NULL;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setExternalStorageDirectories(JNIEnv *env, jobject obj, jobjectArray pathList)
{
    std::string paths;
    int obj_len = env->GetArrayLength(pathList);
    for (int i = 0; i < obj_len; ++i) {
        jstring dir = (jstring)env->GetObjectArrayElement(pathList, i);
        const char* p = env->GetStringUTFChars(dir, 0);
        if (!paths.empty())
            paths += ":";
        paths += p;
        env->ReleaseStringUTFChars(dir, p);
        env->DeleteLocalRef(dir);
    }
    setenv("REICAST_HOME", paths.c_str(), 1);
    gui_refresh_files();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setGameUri(JNIEnv *env,jobject obj,jstring fileName)
{
    if (fileName != NULL)
    {
        // Get filename string from Java
        const char* file_path = env->GetStringUTFChars(fileName, 0);
        NOTICE_LOG(BOOT, "Game Disk URI: '%s'", file_path);
        strncpy(settings.imgread.ImagePath, strlen(file_path) >= 7 && !memcmp(file_path, "file://", 7) ? file_path + 7 : file_path, sizeof(settings.imgread.ImagePath));
        settings.imgread.ImagePath[sizeof(settings.imgread.ImagePath) - 1] = '\0';
        env->ReleaseStringUTFChars(fileName, file_path);
        // TODO game paused/settings/...
        if (game_started) {
            dc_stop();
            gui_state = GuiState::Main;
       		dc_reset(true);
        }
    }
}

//stuff for microphone
jobject sipemu;
jmethodID getmicdata;
jmethodID startRecordingMid;
jmethodID stopRecordingMid;

//stuff for audio
jshortArray jsamples;
jmethodID writeBufferMid;
jmethodID audioInitMid;
jmethodID audioTermMid;
static jobject g_audioBackend;

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)
{
    sipemu = env->NewGlobalRef(sip);
    getmicdata = env->GetMethodID(env->GetObjectClass(sipemu),"getData","(I)[B");
    startRecordingMid = env->GetMethodID(env->GetObjectClass(sipemu),"startRecording","(I)V");
    stopRecordingMid = env->GetMethodID(env->GetObjectClass(sipemu),"stopRecording","()V");
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_pause(JNIEnv *env,jobject obj)
{
    if (game_started)
    {
        dc_stop();
        game_started = true; // restart when resumed
        if (config::AutoSavestate)
            dc_savestate();
    }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_resume(JNIEnv *env,jobject obj)
{
    if (game_started)
        dc_resume();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_stop(JNIEnv *env,jobject obj)
{
    if (dc_is_running()) {
        dc_stop();
        if (config::AutoSavestate)
            dc_savestate();
    }
    dc_term_game();
    gui_state = GuiState::Main;
    settings.imgread.ImagePath[0] = '\0';
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_destroy(JNIEnv *env,jobject obj)
{
    dc_term();
}

JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_send(JNIEnv *env,jobject obj,jint cmd, jint param)
{
    if (cmd==0)
    {
        if (param==0)
        {
            KillTex=true;
            INFO_LOG(RENDERER, "Killing texture cache");
        }
    }
    return 0;
}

JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_data(JNIEnv *env, jobject obj, jint id, jbyteArray d)
{
    return 0;
}

static void *render_thread_func(void *)
{
#ifdef USE_VULKAN
	theVulkanContext.SetWindow((void *)g_window, nullptr);
#endif
	theGLContext.SetNativeWindow((EGLNativeWindowType)g_window);
	InitRenderApi();

	mainui_loop();

	TermRenderApi();
	ANativeWindow_release(g_window);
    g_window = NULL;

    return NULL;
}

static cThread render_thread(render_thread_func, NULL);

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitNative(JNIEnv * env, jobject obj, jobject surface, jint width, jint height)
{
	if (render_thread.thread.joinable())
	{
		if (surface == NULL)
		{
			mainui_stop();
	        render_thread.WaitToEnd();
		}
		else
		{
		    screen_width = width;
		    screen_height = height;
		    mainui_reinit();
		}
	}
	else if (surface != NULL)
	{
        g_window = ANativeWindow_fromSurface(env, surface);
        render_thread.Start();
	}
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitJava(JNIEnv * env, jobject obj, jint width, jint height)
{
    screen_width = width;
    screen_height = height;
    mainui_init();
}

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_rendframeJava(JNIEnv *env,jobject obj)
{
    return (jboolean)mainui_rend_frame();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendtermJava(JNIEnv * env, jobject obj)
{
    mainui_term();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vjoy(JNIEnv * env, jobject obj,int id,float x, float y, float w, float h)
{
    if (id < ARRAY_SIZE(vjoy_pos))
    {
        vjoy_pos[id][0] = x;
        vjoy_pos[id][1] = y;
        vjoy_pos[id][2] = w;
        vjoy_pos[id][3] = h;
    }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_hideOsd(JNIEnv * env, jobject obj) {
    HideOSD();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_getControllers(JNIEnv *env, jobject obj, jintArray controllers, jobjectArray peripherals)
{
    jint *controllers_body = env->GetIntArrayElements(controllers, 0);
    for (u32 i = 0; i < config::MapleMainDevices.size(); i++)
    	controllers_body[i] = (MapleDeviceType)config::MapleMainDevices[i];
    env->ReleaseIntArrayElements(controllers, controllers_body, 0);

    int obj_len = env->GetArrayLength(peripherals);
    for (int i = 0; i < obj_len; ++i) {
        jintArray port = (jintArray) env->GetObjectArrayElement(peripherals, i);
        jint *items = env->GetIntArrayElements(port, 0);
        items[0] = (MapleDeviceType)config::MapleExpansionDevices[i][0];
        items[1] = (MapleDeviceType)config::MapleExpansionDevices[i][1];
        env->ReleaseIntArrayElements(port, items, 0);
        env->DeleteLocalRef(port);
    }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_guiOpenSettings(JNIEnv *env, jobject obj)
{
    gui_open_settings();
}

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsOpen(JNIEnv *env, jobject obj)
{
    return gui_is_open();
}

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsContentBrowser(JNIEnv *env,jobject obj)
{
    return gui_is_content_browser();
}

// Audio Stuff
static u32 androidaudio_push(const void* frame, u32 amt, bool wait)
{
    jvm_attacher.getEnv()->SetShortArrayRegion(jsamples, 0, amt * 2, (jshort *)frame);
    return jvm_attacher.getEnv()->CallIntMethod(g_audioBackend, writeBufferMid, jsamples, wait);
}

static void androidaudio_init()
{
	jint bufferSize = config::AutoLatency ? 0 : config::AudioBufferSize;
	jvm_attacher.getEnv()->CallVoidMethod(g_audioBackend, audioInitMid, bufferSize);
}

static void androidaudio_term()
{
	jvm_attacher.getEnv()->CallVoidMethod(g_audioBackend, audioTermMid);
}

static bool androidaudio_init_record(u32 sampling_freq)
{
	if (sipemu == nullptr)
		return false;
	jvm_attacher.getEnv()->CallVoidMethod(sipemu, startRecordingMid, sampling_freq);
	return true;
}

static void androidaudio_term_record()
{
	jvm_attacher.getEnv()->CallVoidMethod(sipemu, stopRecordingMid);
}

static u32 androidaudio_record(void *buffer, u32 samples)
{
    jbyteArray jdata = (jbyteArray)jvm_attacher.getEnv()->CallObjectMethod(sipemu, getmicdata, samples);
    if (jdata == NULL)
        return 0;
    jsize size = jvm_attacher.getEnv()->GetArrayLength(jdata);
    samples = std::min(samples, (u32)size * 2);
    jvm_attacher.getEnv()->GetByteArrayRegion(jdata, 0, samples * 2, (jbyte*)buffer);
    jvm_attacher.getEnv()->DeleteLocalRef(jdata);

    return samples;
}

audiobackend_t audiobackend_android = {
        "android", // Slug
        "Android Audio", // Name
        &androidaudio_init,
        &androidaudio_push,
        &androidaudio_term,
        NULL,
		&androidaudio_init_record,
		&androidaudio_record,
		&androidaudio_term_record
};

static bool android = RegisterAudioBackend(&audiobackend_android);


JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_AudioBackend_setInstance(JNIEnv *env, jobject obj, jobject instance)
{
    if (g_audioBackend != NULL)
        env->DeleteGlobalRef(g_audioBackend);
    if (instance == NULL) {
        g_audioBackend = NULL;
        if (jsamples != NULL) {
            env->DeleteGlobalRef(jsamples);
            jsamples = NULL;
        }
    }
    else {
        g_audioBackend = env->NewGlobalRef(instance);
        writeBufferMid = env->GetMethodID(env->GetObjectClass(g_audioBackend), "writeBuffer", "([SZ)I");
        audioInitMid = env->GetMethodID(env->GetObjectClass(g_audioBackend), "init", "(I)V");
        audioTermMid = env->GetMethodID(env->GetObjectClass(g_audioBackend), "term", "()V");
        if (jsamples == NULL) {
            jsamples = env->NewShortArray(SAMPLE_COUNT * 2);
            jsamples = (jshortArray) env->NewGlobalRef(jsamples);
        }
    }
}

void os_DebugBreak()
{
    // TODO: notify the parent thread about it ...

	raise(SIGABRT);
    //pthread_exit(NULL);

    // Attach debugger here to figure out what went wrong
    for(;;) ;
}

void SaveAndroidSettings()
{
    jstring homeDirectory = jvm_attacher.getEnv()->NewStringUTF(get_writable_config_path("").c_str());

    jvm_attacher.getEnv()->CallVoidMethod(g_emulator, saveAndroidSettingsMid, homeDirectory);
    jvm_attacher.getEnv()->DeleteLocalRef(homeDirectory);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_init(JNIEnv *env, jobject obj)
{
    input_device_manager = env->NewGlobalRef(obj);
    input_device_manager_rumble = env->GetMethodID(env->GetObjectClass(obj), "rumble", "(IFFI)Z");
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAdded(JNIEnv *env, jobject obj, jint id, jstring name, jint maple_port, jstring junique_id)
{
    const char* joyname = env->GetStringUTFChars(name,0);
    const char* unique_id = env->GetStringUTFChars(junique_id, 0);
    std::shared_ptr<AndroidGamepadDevice> gamepad = std::make_shared<AndroidGamepadDevice>(maple_port, id, joyname, unique_id);
    AndroidGamepadDevice::AddAndroidGamepad(gamepad);
    env->ReleaseStringUTFChars(name, joyname);
    env->ReleaseStringUTFChars(name, unique_id);
}
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickRemoved(JNIEnv *env, jobject obj, jint id)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
    if (device != NULL)
        AndroidGamepadDevice::RemoveAndroidGamepad(device);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_virtualGamepadEvent(JNIEnv *env, jobject obj, jint kcode, jint joyx, jint joyy, jint lt, jint rt)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(AndroidGamepadDevice::VIRTUAL_GAMEPAD_ID);
    if (device != NULL)
        device->virtual_gamepad_event(kcode, joyx, joyy, lt, rt);
}

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickButtonEvent(JNIEnv *env, jobject obj, jint id, jint key, jboolean pressed)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
    if (device != NULL)
        return device->gamepad_btn_input(key, pressed);
    else
    	return false;

}

static std::map<std::pair<jint, jint>, jint> previous_axis_values;

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAxisEvent(JNIEnv *env, jobject obj, jint id, jint key, jint value)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
    // Only handle Left Stick on an Xbox 360 controller if there was actual
    // motion on the stick, otherwise event can be handled as a DPAD event
    if (device != NULL && previous_axis_values[std::make_pair(id, key)] != value)
    {
    	previous_axis_values[std::make_pair(id, key)] = value;
    	return device->gamepad_axis_input(key, value);
    }
    else
    	return false;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_mouseEvent(JNIEnv *env, jobject obj, jint xpos, jint ypos, jint buttons)
{
	SetMousePosition(xpos, ypos, screen_width, screen_height);
    mo_buttons[0] = 0xFFFF;
    if (buttons & 1)	// Left
    	mo_buttons[0] &= ~4;
    if (buttons & 2)	// Right
    	mo_buttons[0] &= ~2;
    if (buttons & 4)	// Middle
    	mo_buttons[0] &= ~8;
    mouse_gamepad.gamepad_btn_input(1, (buttons & 1) != 0);
    mouse_gamepad.gamepad_btn_input(2, (buttons & 2) != 0);
    mouse_gamepad.gamepad_btn_input(4, (buttons & 4) != 0);
}

static jobject g_activity;
static jmethodID VJoyStartEditingMID;
static jmethodID VJoyStopEditingMID;
static jmethodID VJoyResetEditingMID;

JNIEXPORT void JNICALL Java_com_reicast_emulator_BaseGLActivity_register(JNIEnv *env, jobject obj, jobject activity)
{
    if (g_activity != NULL)
    {
        env->DeleteGlobalRef(g_activity);
        g_activity = NULL;
    }
    if (activity != NULL) {
        g_activity = env->NewGlobalRef(activity);
        VJoyStartEditingMID = env->GetMethodID(env->GetObjectClass(activity), "VJoyStartEditing", "()V");
        VJoyStopEditingMID = env->GetMethodID(env->GetObjectClass(activity), "VJoyStopEditing", "(Z)V");
        VJoyResetEditingMID = env->GetMethodID(env->GetObjectClass(activity), "VJoyResetEditing", "()V");
    }
}

void vjoy_start_editing()
{
    jvm_attacher.getEnv()->CallVoidMethod(g_activity, VJoyStartEditingMID);
}

void vjoy_reset_editing()
{
    jvm_attacher.getEnv()->CallVoidMethod(g_activity, VJoyResetEditingMID);
}

void vjoy_stop_editing(bool canceled)
{
    jvm_attacher.getEnv()->CallVoidMethod(g_activity, VJoyStopEditingMID, canceled);
}

void android_send_logs()
{
    JNIEnv *env = jvm_attacher.getEnv();
    jmethodID generateErrorLogMID = env->GetMethodID(env->GetObjectClass(g_activity), "generateErrorLog", "()V");
    env->CallVoidMethod(g_activity, generateErrorLogMID);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setButtons(JNIEnv *env, jobject obj, jbyteArray data)
{
    u32 len = env->GetArrayLength(data);
    DefaultOSDButtons.resize(len);
    jboolean isCopy;
    jbyte* b = env->GetByteArrayElements(data, &isCopy);
    memcpy(DefaultOSDButtons.data(), b, len);
    env->ReleaseByteArrayElements(data, b, JNI_ABORT);
}
