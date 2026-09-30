/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  softfloat.c
 * @brief VFP replacements for the soft-float helpers linked into the game.
 *
 * libIceAgeAndroid.so is armeabi (v5TE, Tag_FP_arch VFPv2 but soft-float
 * code): every float/double operation in the engine -- terrain, AI, matrices,
 * particles -- is a call into libgcc's ieee754 emulation, statically linked at
 * 0xc1fd0..0xc3168. softfloat_patch() overwrites the entry of each helper with
 * a branch to the function below, which does the same operation on the
 * Cortex-A9 VFP.
 *
 * This file is built with -mfloat-abi=softfp (whole project) so the calling
 * convention is exactly the AEABI helper one: floats/doubles in r0-r3, result
 * in r0/r0:r1. It is built WITHOUT -ffast-math (CMakeLists.txt) so NaN
 * comparisons behave like libgcc.
 *
 * Hook safety (checked against the real .so with objdump, 2026-09-30):
 * - Each hook writes 8 bytes (LDR PC,[PC,#-4] + address). No two hooked entries
 *   are closer than 8 bytes.
 * - The only branches into the first 8 bytes of a hooked entry come from
 *   __gesf2/__lesf2/__gedf2/__ledf2 (into __cmpsf2+4/__cmpdf2+4); those four
 *   are hooked too, so that code never runs.
 * - __aeabi_fsub/frsub/dsub/drsub are NOT hooked: they flip a sign bit and
 *   fall through / branch to the start of fadd/dadd, which is hooked.
 * - __aeabi_cf*cmp* / __aeabi_cd*cmp* (result in CPSR flags) are left alone.
 */

#include <kubridge.h>
#include <stdint.h>
#include <so_util/so_util.h>

#include "utils/logger.h"

extern so_module so_mod;

/* --- float ----------------------------------------------------------------- */

static float vfp_fadd(float a, float b) { return a + b; }
static float vfp_fmul(float a, float b) { return a * b; }
static float vfp_fdiv(float a, float b) { return a / b; }

static float vfp_i2f(int a) { return (float) a; }
static float vfp_ui2f(unsigned a) { return (float) a; }
static int vfp_f2iz(float a) { return (int) a; }
static unsigned vfp_f2uiz(float a) { return (unsigned) a; }

static int vfp_fcmpeq(float a, float b) { return a == b; }
static int vfp_fcmplt(float a, float b) { return a < b; }
static int vfp_fcmple(float a, float b) { return a <= b; }
static int vfp_fcmpge(float a, float b) { return a >= b; }
static int vfp_fcmpgt(float a, float b) { return a > b; }

// libgcc three-way compares: <0, 0, >0; the value for unordered (NaN)
// operands is what makes each variant differ.
static int vfp_gesf2(float a, float b) { return a < b ? -1 : a > b ? 1 : a == b ? 0 : -1; }
static int vfp_lesf2(float a, float b) { return a < b ? -1 : a > b ? 1 : a == b ? 0 : 1; }

/* --- double ---------------------------------------------------------------- */

static double vfp_dadd(double a, double b) { return a + b; }
static double vfp_dmul(double a, double b) { return a * b; }
static double vfp_ddiv(double a, double b) { return a / b; }

static double vfp_i2d(int a) { return (double) a; }
static double vfp_ui2d(unsigned a) { return (double) a; }
static int vfp_d2iz(double a) { return (int) a; }
static unsigned vfp_d2uiz(double a) { return (unsigned) a; }

static double vfp_f2d(float a) { return (double) a; }
static float vfp_d2f(double a) { return (float) a; }

static int vfp_dcmpeq(double a, double b) { return a == b; }
static int vfp_dcmplt(double a, double b) { return a < b; }
static int vfp_dcmple(double a, double b) { return a <= b; }
static int vfp_dcmpge(double a, double b) { return a >= b; }
static int vfp_dcmpgt(double a, double b) { return a > b; }

static int vfp_gedf2(double a, double b) { return a < b ? -1 : a > b ? 1 : a == b ? 0 : -1; }
static int vfp_ledf2(double a, double b) { return a < b ? -1 : a > b ? 1 : a == b ? 0 : 1; }

/* --- hooking --------------------------------------------------------------- */

static const struct {
    const char *name;   // one exported name per entry (aliases share it)
    void *fn;
} hooks[] = {
    { "__aeabi_fadd",   vfp_fadd   },  // + __addsf3; fsub/frsub fall into it
    { "__aeabi_fmul",   vfp_fmul   },  // + __mulsf3
    { "__aeabi_fdiv",   vfp_fdiv   },  // + __divsf3
    { "__aeabi_i2f",    vfp_i2f    },  // + __floatsisf
    { "__aeabi_ui2f",   vfp_ui2f   },  // + __floatunsisf
    { "__aeabi_f2iz",   vfp_f2iz   },  // + __fixsfsi
    { "__aeabi_f2uiz",  vfp_f2uiz  },  // + __fixunssfsi
    { "__aeabi_fcmpeq", vfp_fcmpeq },
    { "__aeabi_fcmplt", vfp_fcmplt },
    { "__aeabi_fcmple", vfp_fcmple },
    { "__aeabi_fcmpge", vfp_fcmpge },
    { "__aeabi_fcmpgt", vfp_fcmpgt },
    { "__gesf2",        vfp_gesf2  },  // + __gtsf2
    { "__lesf2",        vfp_lesf2  },  // + __ltsf2
    { "__cmpsf2",       vfp_lesf2  },  // + __eqsf2, __nesf2 (unordered -> 1)

    { "__aeabi_dadd",   vfp_dadd   },  // + __adddf3; dsub/drsub fall into it
    { "__aeabi_dmul",   vfp_dmul   },  // + __muldf3
    { "__aeabi_ddiv",   vfp_ddiv   },  // + __divdf3
    { "__aeabi_i2d",    vfp_i2d    },  // + __floatsidf
    { "__aeabi_ui2d",   vfp_ui2d   },  // + __floatunsidf
    { "__aeabi_d2iz",   vfp_d2iz   },  // + __fixdfsi
    { "__aeabi_d2uiz",  vfp_d2uiz  },  // + __fixunsdfsi
    { "__aeabi_f2d",    vfp_f2d    },  // + __extendsfdf2
    { "__aeabi_d2f",    vfp_d2f    },  // + __truncdfsf2
    { "__aeabi_dcmpeq", vfp_dcmpeq },
    { "__aeabi_dcmplt", vfp_dcmplt },
    { "__aeabi_dcmple", vfp_dcmple },
    { "__aeabi_dcmpge", vfp_dcmpge },
    { "__aeabi_dcmpgt", vfp_dcmpgt },
    { "__gedf2",        vfp_gedf2  },  // + __gtdf2
    { "__ledf2",        vfp_ledf2  },  // + __ltdf2
    { "__cmpdf2",       vfp_ledf2  },  // + __eqdf2, __nedf2
};

void softfloat_patch(void) {
    int n = 0;
    for (unsigned i = 0; i < sizeof(hooks) / sizeof(hooks[0]); i++) {
        uintptr_t addr = so_symbol(&so_mod, hooks[i].name);
        // The helpers are ARM code; a Thumb (odd) address would mean a
        // different .so build than the one the hook table was checked on.
        if (!addr || (addr & 1)) {
            l_warn("softfloat: %s not hooked (addr 0x%08x)", hooks[i].name, (unsigned) addr);
            continue;
        }
        hook_arm(addr, (uintptr_t) hooks[i].fn);
        n++;
    }
    l_success("softfloat: %d libgcc helpers redirected to VFP.", n);
}
