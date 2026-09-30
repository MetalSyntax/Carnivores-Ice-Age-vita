/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  main.c
 * @brief Drives libIceAgeAndroid.so the way the Android Java layer does.
 *
 * Android order (decompiled/apk_jadx/.../IceAgeAndroid.java,
 * IceAgeRenderer.java), reproduced here on a single thread:
 *   onCreate            System.loadLibrary(fmodex, AmazonGamesJni, IceAgeAndroid)
 *   postDownloadInit    nativeSetBundlesPaths(obb, obb)
 *   continueCreating    FacebookWrapper/SocialUtils/... nativeInit(this)
 *                       nativeApplicationDidFinishLaunching(extStorage, apk, filesDir)
 *   GL thread           createFramebuffer(w, h), nativeResize(w, h),
 *                       layoutSubviews() every frame
 *   billing callback    nativeSetBundlesPurchasedState(pack1, pack2)
 *   onWindowFocus       FMODAudioDevice.start() (before continueCreating)
 *
 * createFramebuffer() is called *before* nativeApplicationDidFinishLaunching()
 * here: it calls TexManager_ReloadAllTextures(), and with a GL context already
 * current during InitGame() on Vita that would upload every texture twice.
 */

#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/settings.h"
#include "utils/utils.h"
#include "utils/dialog.h"
#include "reimpl/audio.h"
#include "input.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include <stdio.h>
#include <string.h>

int _newlib_heap_size_user = 256 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod_fmod;
so_module so_mod;

// Placeholder Java objects: the engine keeps them (NewGlobalRef) and only
// ever uses them as receivers for Call*Method, never dereferences them.
static int activity_placeholder, surface_placeholder, renderer_placeholder,
           social_placeholder, facebook_placeholder, fyber_placeholder,
           mopub_placeholder;
jobject activity_obj = (jobject) &activity_placeholder;
jobject surface_obj  = (jobject) &surface_placeholder;

#define SCREEN_W 960
#define SCREEN_H 544

typedef void (*jni_void_fn)(JNIEnv *env, jobject thiz);

static void *sym(const char *name, int required) {
    void *p = (void *) so_symbol(&so_mod, name);
    if (!p) {
        if (required)
            fatal_error("Error: %s not found in libIceAgeAndroid.so", name);
        l_warn("Optional symbol %s not found", name);
    }
    return p;
}

// First existing path, or NULL. The Google Play expansion (main.obb) holds
// both packs; the standalone bundle APKs hold one each.
static const char *find_bundle(const char *a, const char *b, const char *c) {
    if (file_exists(a)) return a;
    if (file_exists(b)) return b;
    if (file_exists(c)) return c;
    return NULL;
}

static void call_init(const char *name, jobject obj) {
    jni_void_fn fn = sym(name, 0);
    if (fn) {
        l_info("Calling %s", name);
        fn(&jni, obj);
    }
}

int main() {
    soloader_init_all();

    int (*JNI_OnLoad)(void *jvm) = sym("JNI_OnLoad", 1);
    l_info("JNI_OnLoad -> 0x%x", JNI_OnLoad(&jvm));

    void (*nativeSetBundlesPaths)(JNIEnv *, jobject, jstring, jstring) =
            sym("Java_com_tatem_iceage_IceAgeAndroid_nativeSetBundlesPaths", 1);
    void (*nativeApplicationDidFinishLaunching)(JNIEnv *, jobject, jstring, jstring, jstring) =
            sym("Java_com_tatem_iceage_IceAgeAndroid_nativeApplicationDidFinishLaunching", 1);
    void (*nativeSetBundlesPurchasedState)(JNIEnv *, jobject, jboolean, jboolean) =
            sym("Java_com_tatem_iceage_IceAgeAndroid_nativeSetBundlesPurchasedState", 0);
    void (*createFramebuffer)(JNIEnv *, jobject, jint, jint) =
            sym("Java_com_tatem_iceage_IceAgeRenderer_createFramebuffer", 1);
    void (*nativeResize)(JNIEnv *, jobject, jint, jint) =
            sym("Java_com_tatem_iceage_IceAgeRenderer_nativeResize", 1);
    jni_void_fn layoutSubviews = sym("Java_com_tatem_iceage_IceAgeRenderer_layoutSubviews", 1);

    // Assets: the engine opens the APK itself with its bundled libzip
    // (Files_OpenFileOfType -> zip_open/zip_fopen(ZIP_FL_NODIR)), then the two
    // content bundles as fallbacks. Pack 1 (areas 3-4, sniper rifle) and
    // pack 2 (area 6, double-barreled shotgun, crossbow) are not in the APK;
    // without them those weapons have no model (see patch.c).
    if (!file_exists(APK_PATH)) {
        fatal_error("Looks like you haven't installed the data files for this "
                    "port. Please copy the original APK to %s", APK_PATH);
    }
    const char *bundle1 = find_bundle(DATA_PATH "main.obb", DATA_PATH "bundle1.apk",
                                      DATA_PATH "CarnivoresBundleOne.apk");
    const char *bundle2 = find_bundle(DATA_PATH "main.obb", DATA_PATH "bundle2.apk",
                                      DATA_PATH "CarnivoresBundleTwo.apk");
    l_info("APK: %s, bundle 1: %s, bundle 2: %s", APK_PATH,
           bundle1 ? bundle1 : "(none)", bundle2 ? bundle2 : "(none)");

    gl_init();
    l_success("vitaGL initialized (%dx%d).", SCREEN_W, SCREEN_H);

    // A missing bundle gets a path that does not exist: the zip_open() cache in
    // patch.c answers "not available" for it without touching the card.
    nativeSetBundlesPaths(&jni, activity_obj,
                          jni->NewStringUTF(&jni, bundle1 ? bundle1 : DATA_PATH "bundle1.apk"),
                          jni->NewStringUTF(&jni, bundle2 ? bundle2 : DATA_PATH "bundle2.apk"));

    call_init("Java_com_tatem_iceage_utils_FacebookWrapper_nativeInit", (jobject) &facebook_placeholder);
    call_init("Java_com_tatem_iceage_utils_SocialUtils_nativeInit", (jobject) &social_placeholder);
    call_init("Java_com_tatem_iceage_utils_FyberManager_nativeInit", (jobject) &fyber_placeholder);
    call_init("Java_com_tatem_iceage_utils_MoPubManager_nativeInit", (jobject) &mopub_placeholder);

    l_info("createFramebuffer(%d, %d)", SCREEN_W, SCREEN_H);
    createFramebuffer(&jni, (jobject) &renderer_placeholder, SCREEN_W, SCREEN_H);

    // Environment.getExternalStorageDirectory() -> photos in <ext>/.iceage/photos
    // ApplicationInfo.sourceDir                 -> the APK
    // getFilesDir()                             -> CarnivoresData.dt lives here
    // (the engine appends "/<name>", so no trailing slash)
    char data_dir[256];
    snprintf(data_dir, sizeof(data_dir), "%s", DATA_PATH);
    size_t len = strlen(data_dir);
    if (len > 0 && data_dir[len - 1] == '/')
        data_dir[len - 1] = '\0';

    jstring ext_str = jni->NewStringUTF(&jni, data_dir);
    jstring apk_str = jni->NewStringUTF(&jni, APK_PATH);
    jstring files_str = jni->NewStringUTF(&jni, data_dir);

    // FMODAudioDevice.start() runs from onWindowFocusChanged(), i.e. before
    // continueCreating() -> Sounds_Init(); the pump thread just polls
    // fmodGetInfo() until FMOD's AudioTrack output exists.
    audio_start();

    l_info("nativeApplicationDidFinishLaunching(%s, %s, %s)", data_dir, APK_PATH, data_dir);
    nativeApplicationDidFinishLaunching(&jni, activity_obj, ext_str, apk_str, files_str);
    l_success("nativeApplicationDidFinishLaunching returned.");

    nativeResize(&jni, (jobject) &renderer_placeholder, SCREEN_W, SCREEN_H);

    if (nativeSetBundlesPurchasedState && setting_unlockBundles) {
        l_info("Marking both content bundles as owned (unlock_bundles=1).");
        nativeSetBundlesPurchasedState(&jni, activity_obj, JNI_TRUE, JNI_TRUE);
    }

    input_init();

    l_info("Entering main loop.");
    log_set_buffered(1);

    // show_fps: every 5 s, the frame rate plus where the frame time goes --
    // "engine" is the CPU side (Process + Render issuing GL calls), "swap" is
    // mostly waiting for the GPU / vblank.
    uint64_t fps_t0 = sceKernelGetProcessTimeWide();
    uint64_t engine_us = 0, swap_us = 0;
    unsigned frames = 0;

    while (1) {
        uint64_t t0 = sceKernelGetProcessTimeWide();
        input_update();
        layoutSubviews(&jni, (jobject) &renderer_placeholder);
        uint64_t t1 = sceKernelGetProcessTimeWide();
        gl_swap();
        uint64_t t2 = sceKernelGetProcessTimeWide();

        engine_us += t1 - t0;
        swap_us += t2 - t1;
        frames++;
        if (t2 - fps_t0 >= 5000000) {
            if (setting_showFps)
                l_info("fps: %.1f | engine %.1f ms, swap %.1f ms per frame",
                       frames * 1000000.0 / (double) (t2 - fps_t0),
                       engine_us / 1000.0 / frames, swap_us / 1000.0 / frames);
            log_flush();
            fps_t0 = t2;
            engine_us = swap_us = 0;
            frames = 0;
        }
    }

    audio_stop();
    sceKernelExitDeleteThread(0);
}
