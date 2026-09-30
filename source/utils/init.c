/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021-2022 Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/init.h"

#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/settings.h"

#include <string.h>

#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/kernel/clib.h>
#include <psp2/power.h>
#include <psp2/io/stat.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <fios/fios.h>

// Base addresses for the two Android .so modules. libfmodex.so spans
// 0x11BDE0 bytes of memory (text + bss), libIceAgeAndroid.so spans ~0x1620000
// (22 MB, mostly .bss), so the gaps below never overlap.
#define LOAD_ADDRESS_FMOD 0x98000000
#define LOAD_ADDRESS_GAME 0x98400000

extern so_module so_mod_fmod;
extern so_module so_mod;

static void load_module(so_module *mod, const char *path, uintptr_t addr, int patch) {
    if (!file_exists(path)) {
        fatal_error("Looks like you haven't installed the data files for this "
                    "port, or they are in an incorrect location. Please make "
                    "sure that you have %s file exactly at that path.", path);
    }

    l_info("Loading %s at 0x%08X", path, (unsigned) addr);
    if (so_file_load(mod, path, addr) < 0) {
        l_fatal("SO could not be loaded: %s", path);
        fatal_error("Error: could not load %s.", path);
    }

    so_relocate(mod);
    l_success("%s relocated.", path);

    // libIceAgeAndroid.so links FMOD_* against libfmodex.so via
    // so_resolve_link(), so libfmodex.so must already be loaded here.
    resolve_imports(mod);
    l_success("%s imports resolved.", path);

    if (patch) {
        so_patch();
        l_success("%s patched.", path);
    }

    so_flush_caches(mod);
    so_initialize(mod);
    l_success("%s initialized.", path);
}

void soloader_init_all() {
	// Launch `app0:configurator.bin` on `-config` init param
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
    SceAppUtilAppEventParam eventParam;
    sceClibMemset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
    sceAppUtilReceiveAppEvent(&eventParam);
    if (eventParam.type == 0x05) {
        char buffer[2048];
        sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
        if (strstr(buffer, "-config"))
            sceAppMgrLoadExec("app0:/configurator.bin", NULL, NULL);
    }

    // Set default overclock values
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    // Working dirs the engine expects: basedir (CarnivoresData.dt save file)
    // and <external storage>/.iceage/photos (in-game camera shots).
    sceIoMkdir(DATA_PATH, 0777);
    sceIoMkdir(DATA_PATH "logs", 0777);
    sceIoMkdir(DATA_PATH "saves", 0777);
    sceIoMkdir(DATA_PATH ".iceage", 0777);
    sceIoMkdir(DATA_PATH ".iceage/photos", 0777);

#ifdef USE_SCELIBC_IO
    if (fios_init(DATA_PATH) == 0)
        l_success("FIOS initialized.");
#endif

    if (!module_loaded("kubridge")) {
        l_fatal("kubridge is not loaded.");
        fatal_error("Error: kubridge.skprx is not installed.");
    }
    l_success("kubridge check passed.");

    settings_load();
    l_success("Settings loaded.");

    // libAmazonGamesJni.so is also in the APK, but libIceAgeAndroid.so imports
    // no symbol from it (checked with nm -D) and its only role is the Amazon
    // GameCircle Java bridge, so it is intentionally not loaded.
    load_module(&so_mod_fmod, FMOD_SO_PATH, LOAD_ADDRESS_FMOD, 0);
    load_module(&so_mod, SO_PATH, LOAD_ADDRESS_GAME, 1);

    gl_preload();
    l_success("OpenGL preloaded.");

    jni_init();
    l_success("FalsoJNI initialized.");
}
