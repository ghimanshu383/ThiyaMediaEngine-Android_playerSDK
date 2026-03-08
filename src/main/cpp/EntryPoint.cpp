//
// Created by ghima on 12-02-2026.
//
#include <jni.h>
#include <android_native_app_glue.h>
#include "VideoProcessor.h"
#include "Util.h"
#include "imgui/imgui_impl_android.h"
#include <memory>

static bool hasWindow = false;
static bool hasFocus = false;

static int get_video_fd(android_app *app) {
    JavaVM *vm = app->activity->vm;
    ANativeActivity *activity = app->activity;
    JNIEnv *env = nullptr;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return -1;
        }
    }
    jobject activityObject = activity->clazz;

    jclass activityClass = env->GetObjectClass(activityObject);
    jmethodID getIntent = env->GetMethodID(activityClass, "getIntent",
                                           "()Landroid/content/Intent;");

    jobject intent = env->CallObjectMethod(activityObject, getIntent);
    jclass intentClass = env->GetObjectClass(intent);

    jmethodID getIntExtra = env->GetMethodID(intentClass, "getIntExtra", "(Ljava/lang/String;I)I");
    jstring fdExtraString = env->NewStringUTF("fd");
    int fd = env->CallIntMethod(intent, getIntExtra, fdExtraString, -1);
    LOG_INFO("Fd Received from JNI Bridge %d", fd);
    return fd;
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    return ImGui_ImplAndroid_HandleInputEvent(event);
}

static void handle_command(android_app *app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW : {
            LOG_INFO("The init command received ");
            ANativeWindow *window = app->window;
            if (window) {
                int processFd = get_video_fd(app);
                ve::VideoProcessor::get_instance(app);
                ve::VideoProcessor::get_instance(app)->create_swapchain_graphics_context(app);
                ve::VideoProcessor::get_instance(app)->start_media_decoder(processFd);
                LOG_INFO("Native Window Started");
                hasWindow = true;
            }
            break;
        }
        case APP_CMD_TERM_WINDOW : {
            LOG_INFO("The Term command received");
            hasFocus = false;
            hasWindow = false;
            ve::VideoProcessor::get_instance(app)->on_term_window();
            break;
        }
        case APP_CMD_GAINED_FOCUS : {
            LOG_INFO("window Focus gained");
            hasFocus = true;
            break;
        }
    }
}

void android_main(struct android_app *app) {
    LOG_INFO("Android Main started");
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;
    while (true) {
        int events;
        android_poll_source *source;

        while (ALooper_pollAll(16, nullptr, &events, (void **) &source) >= 0) {
            if (source) {
                source->process(app, source);
            }
            if (app->destroyRequested) {
                LOG_INFO("App Destroyed");
                hasWindow = false;
                hasFocus = false;
                return;
            }
        }
        if (hasWindow && hasFocus) {
            ve::VideoProcessor::get_instance(nullptr)->play();
        }
    }
}