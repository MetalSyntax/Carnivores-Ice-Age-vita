/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>

#include "input.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <stdint.h>
#include <string.h>

extern so_module so_mod;

// Calls the original of a void function hooked with hook_addr().
#define SO_CONTINUE_VOID(h, fn_type, ...) do { \
        kuKernelCpuUnrestrictedMemcpy((void *) h.addr, h.orig_instr, sizeof(h.orig_instr)); \
        kuKernelFlushCaches((void *) h.addr, sizeof(h.orig_instr)); \
        ((fn_type) (h.thumb_addr ? h.thumb_addr : h.addr))(__VA_ARGS__); \
        kuKernelCpuUnrestrictedMemcpy((void *) h.addr, h.patch_instr, sizeof(h.patch_instr)); \
        kuKernelFlushCaches((void *) h.addr, sizeof(h.patch_instr)); \
    } while (0)

/* --- libzip: inlined bionic ferror() ---------------------------------------
 * The engine's libzip was built against bionic, where ferror(fp) is a macro
 * reading fp->_flags (offset 0xc) & __SERR (0x40). Our FILE*s come from
 * SceLibc, whose struct has something else there: the bit happened to be clear
 * for the APK and set for the content bundles, so zip_open() failed on them
 * with ZIP_ER_READ ("Failed to open archive"). Each site is Thumb
 * `ldrh r3, [r3, #12]` (0x899b) followed by `& 0x40`; it becomes
 * `movs r3, #0` (0x2300), i.e. "no error". Real read errors are still caught
 * through fread()'s return value. Sites found with objdump (all of them). */

static const uint32_t ferror_sites[] = {
    0x9fda0,    // _zip_cdir_write
    0xa0524,    // _zip_dirent_write
    0xa1998,    // zip_open: _zip_readcdir, after fseeko
    0xa19be,    // zip_open: _zip_readcdir, consistency check
    0xa2098,    // zip_open: _zip_find_central_dir, after fread
};

static void patch_inlined_ferror(void) {
    int n = 0;
    for (unsigned i = 0; i < sizeof(ferror_sites) / sizeof(ferror_sites[0]); i++) {
        uint16_t *insn = (uint16_t *) (so_mod.text_base + ferror_sites[i]);
        if (*insn != 0x899b) {
            l_warn("zip: unexpected opcode 0x%04x at 0x%x, ferror site not patched", *insn,
                   (unsigned) ferror_sites[i]);
            continue;
        }
        uint16_t movs_r3_0 = 0x2300;
        kuKernelCpuUnrestrictedMemcpy(insn, &movs_r3_0, sizeof(movs_r3_0));
        n++;
    }
    if (n != (int) (sizeof(ferror_sites) / sizeof(ferror_sites[0])))
        l_warn("zip: %d inlined ferror() sites patched", n);
}

/* --- libzip archive cache ---------------------------------------------------
 * Files_OpenFileOfType() looks every file up in the APK and, when it is not
 * there, in bundle1 then bundle2 -- zip_open()ing each bundle on every miss
 * and zip_close()ing both after every successful read. Every texture misses at
 * least once (.crthd -> .tga -> .crt), so the bundles' central directories were
 * re-parsed hundreds of times while loading. Keep each archive open for the
 * whole session instead (the engine never writes to them), and remember paths
 * that do not exist so a missing bundle costs nothing. */

#define ZIP_ER_OPEN 11

typedef void *(*zip_open_fn)(const char *path, int flags, int *errorp);
typedef int (*zip_close_fn)(void *za);

static so_hook zip_open_hook, zip_close_hook;

static struct {
    char path[256];
    void *za;           // NULL: the archive does not exist / cannot be opened
} zip_cache[4];
static int zip_cache_count;

static void *zip_open_cached(const char *path, int flags, int *errorp) {
    for (int i = 0; i < zip_cache_count; i++) {
        if (strcmp(zip_cache[i].path, path) == 0) {
            if (!zip_cache[i].za && errorp)
                *errorp = ZIP_ER_OPEN;
            return zip_cache[i].za;
        }
    }

    void *za = NULL;
    if (file_exists(path)) {
        // Restore, call and re-hook by hand: SO_CONTINUE needs a return type
        // and the result has to be kept.
        kuKernelCpuUnrestrictedMemcpy((void *) zip_open_hook.addr, zip_open_hook.orig_instr, 8);
        kuKernelFlushCaches((void *) zip_open_hook.addr, 8);
        za = ((zip_open_fn) zip_open_hook.thumb_addr)(path, flags, errorp);
        kuKernelCpuUnrestrictedMemcpy((void *) zip_open_hook.addr, zip_open_hook.patch_instr, 8);
        kuKernelFlushCaches((void *) zip_open_hook.addr, 8);
    } else if (errorp) {
        *errorp = ZIP_ER_OPEN;
    }

    if (zip_cache_count < (int) (sizeof(zip_cache) / sizeof(zip_cache[0]))) {
        strncpy(zip_cache[zip_cache_count].path, path, sizeof(zip_cache[0].path) - 1);
        zip_cache[zip_cache_count].za = za;
        zip_cache_count++;
    }
    l_info("zip: %s %s", path, za ? "opened (kept open)" : "not available");
    return za;
}

static int zip_close_cached(void *za) {
    for (int i = 0; i < zip_cache_count; i++) {
        if (za && zip_cache[i].za == za)
            return 0; // cached: stays open
    }
    kuKernelCpuUnrestrictedMemcpy((void *) zip_close_hook.addr, zip_close_hook.orig_instr, 8);
    kuKernelFlushCaches((void *) zip_close_hook.addr, 8);
    int r = ((zip_close_fn) zip_close_hook.thumb_addr)(za);
    kuKernelCpuUnrestrictedMemcpy((void *) zip_close_hook.addr, zip_close_hook.patch_instr, 8);
    kuKernelFlushCaches((void *) zip_close_hook.addr, 8);
    return r;
}

/* --- weapons whose files are missing ----------------------------------------
 * CharacterInfo_Load() sets each animation's length to (frames - 1) / kps from
 * its .ani file and leaves it at 0 when the file is missing. Weapons_Animate()
 * then runs `do t -= len; while (len <= t);` -> infinite loop, the game
 * freezes the first time the weapon is drawn (Square / weapon button).
 * shotgun.3dn + shotgun_animation_*.ani are in no Ice Age archive we have
 * (the APK only has shotgun.can); dbsgun/x_bow/sniper only exist in the
 * content bundles. A weapon slot left without valid animations is reloaded with
 * the first model that loads fine; the weapon keeps its own stats (they are
 * indexed by slot), only the 3D model and its animations are borrowed. */

#define CHARINFO_STRIDE   0xf7c
#define CHARINFO_ANIMS    0x24     // int: animation count
#define ANIM_BASE         0x2c     // animation i at + i * ANIM_STRIDE
#define ANIM_STRIDE       0x34
#define ANIM_FRAMES       0x24     // relative to ANIM_BASE: +0x50 of the record
#define ANIM_LENGTH       0x2c     // +0x58: float, seconds
#define ANIM_DATA         0x30     // +0x5c: vertex frames

typedef void (*charinfo_load_fn)(int index, const char *name);

static so_hook charinfo_hook;
static uint8_t *characters_info;

// Every animation file was found and loaded.
static int charinfo_valid(int index) {
    uint8_t *c = characters_info + index * CHARINFO_STRIDE;
    int count = *(int *) (c + CHARINFO_ANIMS);
    if (count <= 0)
        return 0;
    for (int i = 0; i < count; i++) {
        uint8_t *a = c + ANIM_BASE + i * ANIM_STRIDE;
        if (*(int *) (a + ANIM_FRAMES) <= 0 || !*(void **) (a + ANIM_DATA))
            return 0;
    }
    return 1;
}

// A zero length also comes from legit single-frame animations
// ((frames - 1) / kps); either way it is what hangs the animation loops.
// 10 ms ends within one frame, like 0 would, without looping forever.
static void charinfo_fix_lengths(int index) {
    uint8_t *c = characters_info + index * CHARINFO_STRIDE;
    int count = *(int *) (c + CHARINFO_ANIMS);
    for (int i = 0; i < count && i < 32; i++) {
        float *len = (float *) (c + ANIM_BASE + i * ANIM_STRIDE + ANIM_LENGTH);
        if (!(*len > 0.0f))
            *len = 0.01f;
    }
}

// Weapon slots: 0 pistol, 1 shotgun, 2 dbsgun, 3 x_bow, 4 rifle, 5 sniper,
// 0x22 the photo-mode camera (pistol model) -- see Game_LoadStep case 5.
static int is_weapon_slot(int index) {
    return (index >= 0 && index <= 5) || index == 0x22;
}

static void CharacterInfo_Load_hook(int index, const char *name) {
    static const char *fallbacks[] = { "dbsgun", "rifle", "pistol" };

    SO_CONTINUE_VOID(charinfo_hook, charinfo_load_fn, index, name);
    if (charinfo_valid(index)) {
        charinfo_fix_lengths(index);
        return;
    }

    if (is_weapon_slot(index)) {
        for (unsigned i = 0; i < sizeof(fallbacks) / sizeof(fallbacks[0]); i++) {
            if (strcmp(fallbacks[i], name) == 0)
                continue;
            SO_CONTINUE_VOID(charinfo_hook, charinfo_load_fn, index, fallbacks[i]);
            if (charinfo_valid(index)) {
                charinfo_fix_lengths(index);
                l_warn("assets: weapon '%s' has no model/animations, using '%s' instead",
                       name, fallbacks[i]);
                return;
            }
        }
    }

    // Last resort (non-weapon, or no fallback loaded).
    charinfo_fix_lengths(index);
    l_warn("assets: character '%s' (slot %d) is missing animation files", name, index);
}

void so_patch(void) {
    // Right stick camera: feed the stick into the engine's own camera input.
    input_patch();

    patch_inlined_ferror();

    uintptr_t zo = so_symbol(&so_mod, "zip_open");
    uintptr_t zc = so_symbol(&so_mod, "zip_close");
    if (zo && zc && (zo & 1) && (zc & 1)) { // both Thumb (checked with objdump)
        zip_open_hook = hook_addr(zo, (uintptr_t) &zip_open_cached);
        zip_close_hook = hook_addr(zc, (uintptr_t) &zip_close_cached);
    } else {
        l_warn("zip: zip_open/zip_close not hooked");
    }

    characters_info = (uint8_t *) so_symbol(&so_mod, "characters_info");
    uintptr_t cl = so_symbol(&so_mod, "_Z18CharacterInfo_LoadiPc");
    if (characters_info && cl && !(cl & 1))
        charinfo_hook = hook_addr(cl, (uintptr_t) &CharacterInfo_Load_hook);
    else
        l_warn("assets: CharacterInfo_Load not hooked");
}
