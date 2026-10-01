/*
 * Copyright (C) 2026 Carnivores Ice Age Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  java.c
 * @brief FalsoJNI implementation of the Java side libIceAgeAndroid.so talks to.
 *
 * Every method below is one the .so actually resolves with GetMethodID
 * (string literals in decompiled/libIceAgeAndroid_armeabi/ghidra/out_ghidra.c),
 * not the whole APK's Java surface. Receivers: IceAgeAndroid (the `activity`
 * global), its `internet` (InternetUtils) and `purchaseManager` fields, and the
 * SocialUtils / FacebookWrapper / FyberManager / MoPubManager objects handed
 * to the matching nativeInit() calls in main.c.
 *
 * Only two calls return an object whose result the engine dereferences:
 * getCurrentLanguage() (Locale_Init strlen()s it) and getSnapshot() (parsed
 * with JsonBox) -- both must return a real string, never NULL.
 */

#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>

#include <psp2/apputil.h>
#include <psp2/system_param.h>

#include <string.h>

#include "utils/logger.h"
#include "utils/settings.h"

/*
 * JNI Methods
 */

enum {
    // IceAgeAndroid
    M_GET_CURRENT_LANGUAGE = 1,
    M_GET_SNAPSHOT,
    M_SAVE_SNAPSHOT,
    M_IS_SIGNED_IN,
    M_BEGIN_USER_INITIATED_SIGN_IN,
    M_SIGN_OUT,
    M_SHOW_SIGN_IN_PROPOSE,
    M_SHOW_SIGN_IN_PROPOSE_AT_FIRST,
    M_SHOW_ALERT_DIALOG,
    M_ON_LOADING_COMPLETED,
    M_SET_TUTORIAL_FILE,
    M_SHOW_TUTORIAL,
    M_HIDE_TUTORIAL,
    M_SHOW_IN_APP_INFO,
    M_HIDE_IN_APP_INFO,
    M_SHOW_GALLERY,
    M_HIDE_GALLERY,
    M_PUT_FRAME_ON_PHOTO,
    M_SHOW_SOCIAL_BUTTON,
    M_HIDE_SOCIAL_BUTTON,
    M_IS_IN_MENU_MODE,
    M_IS_IN_GAME_MODE,
    M_OPEN_ACHIEVEMENTS,
    M_UNLOCK_ACHIEVEMENT,
    M_SYNC_PROGRESS,
    M_SYNCHRONIZE_GAME_DATA,
    // InternetUtils
    M_IS_ONLINE,
    M_OPEN_MORE_GAMES,
    M_OPEN_HOT_APP_BANNER,
    M_OPEN_TAPJOY_OFFERS,
    // PurchaseManager
    M_REQUEST_PURCHASE,
    // SocialUtils
    M_IS_AGS_INITIALIZED,
    M_SEND_FLURRY_EVENT,
    M_SEND_GA_EVENT,
    M_SEND_GA_SCREEN,
    // FacebookWrapper
    M_LOG_IN,
    M_LOG_OUT,
    M_PUBLISH_FEED,
    // FyberManager / MoPubManager
    M_SHOW_AD,
    M_IS_FYBER_ADS_AVALIABLE,
    M_IS_REVIVE_AVAILABLE,
};

NameToMethodID nameToMethodId[] = {
    { M_GET_CURRENT_LANGUAGE,          "getCurrentLanguage",       METHOD_TYPE_OBJECT },
    { M_GET_SNAPSHOT,                  "getSnapshot",              METHOD_TYPE_OBJECT },
    { M_SAVE_SNAPSHOT,                 "saveSnapshot",             METHOD_TYPE_VOID },
    { M_IS_SIGNED_IN,                  "isSignedIn",               METHOD_TYPE_BOOLEAN },
    { M_BEGIN_USER_INITIATED_SIGN_IN,  "beginUserInitiatedSignIn", METHOD_TYPE_VOID },
    { M_SIGN_OUT,                      "signOut",                  METHOD_TYPE_VOID },
    { M_SHOW_SIGN_IN_PROPOSE,          "showSignInPropose",        METHOD_TYPE_VOID },
    { M_SHOW_SIGN_IN_PROPOSE_AT_FIRST, "showSignInProposeAtFirst", METHOD_TYPE_VOID },
    { M_SHOW_ALERT_DIALOG,             "showAlertDialog",          METHOD_TYPE_VOID },
    { M_ON_LOADING_COMPLETED,          "onLoadingCompleted",       METHOD_TYPE_VOID },
    { M_SET_TUTORIAL_FILE,             "setTutorialFile",          METHOD_TYPE_VOID },
    { M_SHOW_TUTORIAL,                 "showTutorial",             METHOD_TYPE_VOID },
    { M_HIDE_TUTORIAL,                 "hideTutorial",             METHOD_TYPE_VOID },
    { M_SHOW_IN_APP_INFO,              "showInAppInfo",            METHOD_TYPE_VOID },
    { M_HIDE_IN_APP_INFO,              "hideInAppInfo",            METHOD_TYPE_VOID },
    { M_SHOW_GALLERY,                  "showGallery",              METHOD_TYPE_VOID },
    { M_HIDE_GALLERY,                  "hideGallery",              METHOD_TYPE_VOID },
    { M_PUT_FRAME_ON_PHOTO,            "putFrameOnPhoto",          METHOD_TYPE_VOID },
    { M_SHOW_SOCIAL_BUTTON,            "showSocialButton",         METHOD_TYPE_VOID },
    { M_HIDE_SOCIAL_BUTTON,            "hideSocialButton",         METHOD_TYPE_VOID },
    { M_IS_IN_MENU_MODE,               "isInMenuMode",             METHOD_TYPE_VOID },
    { M_IS_IN_GAME_MODE,               "isInGameMode",             METHOD_TYPE_VOID },
    { M_OPEN_ACHIEVEMENTS,             "openAchievements",         METHOD_TYPE_VOID },
    { M_UNLOCK_ACHIEVEMENT,            "unlockAchievement",        METHOD_TYPE_VOID },
    { M_SYNC_PROGRESS,                 "syncProgress",             METHOD_TYPE_VOID },
    { M_SYNCHRONIZE_GAME_DATA,         "synchronizeGameData",      METHOD_TYPE_VOID },
    { M_IS_ONLINE,                     "isOnline",                 METHOD_TYPE_BOOLEAN },
    { M_OPEN_MORE_GAMES,               "openMoreGames",            METHOD_TYPE_VOID },
    { M_OPEN_HOT_APP_BANNER,           "openHotAppBanner",         METHOD_TYPE_VOID },
    { M_OPEN_TAPJOY_OFFERS,            "openTapJoyOffers",         METHOD_TYPE_VOID },
    { M_REQUEST_PURCHASE,              "requestPurchase",          METHOD_TYPE_BOOLEAN },
    { M_IS_AGS_INITIALIZED,            "isAgsInitialized",         METHOD_TYPE_BOOLEAN },
    { M_SEND_FLURRY_EVENT,             "SendFlurryEvent",          METHOD_TYPE_VOID },
    { M_SEND_GA_EVENT,                 "sendGAEvent",              METHOD_TYPE_VOID },
    { M_SEND_GA_SCREEN,                "sendGAScreen",             METHOD_TYPE_VOID },
    { M_LOG_IN,                        "logIn",                    METHOD_TYPE_VOID },
    { M_LOG_OUT,                       "logOut",                   METHOD_TYPE_VOID },
    { M_PUBLISH_FEED,                  "publishFeed",              METHOD_TYPE_VOID },
    { M_SHOW_AD,                       "showAd",                   METHOD_TYPE_VOID },
    { M_IS_FYBER_ADS_AVALIABLE,        "isFyberAdsAvaliable",      METHOD_TYPE_VOID },
    { M_IS_REVIVE_AVAILABLE,           "isReviveAvailable",        METHOD_TYPE_VOID },
};

/* --- Object ------------------------------------------------------------- */

static const char *vita_language(void) {
    // setting_language: 0 = follow system, 1 en, 2 de, 3 fr, 4 es. The engine
    // only ships en/de/fr/es and falls back to "en" for anything else.
    switch (setting_language) {
        case 1: return "en";
        case 2: return "de";
        case 3: return "fr";
        case 4: return "es";
        default: break;
    }

    int lang = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
    sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &lang);
    switch (lang) {
        case SCE_SYSTEM_PARAM_LANG_GERMAN:     return "de";
        case SCE_SYSTEM_PARAM_LANG_FRENCH:     return "fr";
        case SCE_SYSTEM_PARAM_LANG_SPANISH:    return "es";
        default:                               return "en";
    }
}

// IceAgeAndroid.getCurrentLanguage() -> Locale.getDefault().getLanguage()
static jobject getCurrentLanguage(jmethodID id, va_list args) {
    const char *lang = vita_language();
    l_info("JNI: getCurrentLanguage() -> %s", lang);
    return (jobject) jni->NewStringUTF(&jni, lang);
}

// IceAgeAndroid.getSnapshot(): Google Play Games saved game, never available.
static jobject getSnapshot(jmethodID id, va_list args) {
    l_info("JNI: getSnapshot() -> {}");
    return (jobject) jni->NewStringUTF(&jni, "{}");
}

/* --- Boolean ------------------------------------------------------------ */

static jboolean retFalse(jmethodID id, va_list args) {
    return JNI_FALSE;
}

// PurchaseManager.requestPurchase(String): no store on Vita. Returning false
// makes the engine show its own "Can't connect to the market" notification.
static jboolean requestPurchase(jmethodID id, va_list args) {
    jstring product = va_arg(args, jstring);
    const char *s = product ? jni->GetStringUTFChars(&jni, product, NULL) : NULL;
    l_info("JNI: requestPurchase(%s) -> false (no store)", s ? s : "?");
    if (s) jni->ReleaseStringUTFChars(&jni, product, (char *) s);
    return JNI_FALSE;
}

/* --- Void --------------------------------------------------------------- */

static void noop(jmethodID id, va_list args) {
    l_debug("JNI: void method %d ignored", (int) id);
}

static void logWithString(const char *name, va_list args) {
    jstring str = va_arg(args, jstring);
    const char *s = str ? jni->GetStringUTFChars(&jni, str, NULL) : NULL;
    l_info("JNI: %s(\"%s\")", name, s ? s : "(null)");
    if (s) jni->ReleaseStringUTFChars(&jni, str, (char *) s);
}

static void onLoadingCompleted(jmethodID id, va_list args) {
    // Java hides the splash spinner and shows the GL view; nothing to do.
    l_info("JNI: onLoadingCompleted()");
}

// Help pages are Android WebViews over the GL surface (assets/Help/*.html),
// which the port cannot display: the frame is drawn, the HTML body is not.
static void setTutorialFile(jmethodID id, va_list args) {
    logWithString("setTutorialFile", args);
}

static void showTutorial(jmethodID id, va_list args) {
    l_warn("JNI: showTutorial(): HTML help pages are not rendered on Vita");
}

static void showAlertDialog(jmethodID id, va_list args) {
    jint code = va_arg(args, jint);
    l_info("JNI: showAlertDialog(%d) ignored", (int) code);
}

static void unlockAchievement(jmethodID id, va_list args) {
    jint ach_id = va_arg(args, jint);
    l_info("JNI: unlockAchievement(%d)", (int) ach_id);
}

static void sendFlurryEvent(jmethodID id, va_list args) {
    logWithString("SendFlurryEvent", args);
}

MethodsObject methodsObject[] = {
    { M_GET_CURRENT_LANGUAGE, getCurrentLanguage },
    { M_GET_SNAPSHOT,         getSnapshot },
};

MethodsBoolean methodsBoolean[] = {
    { M_IS_SIGNED_IN,       retFalse },
    { M_IS_ONLINE,          retFalse },
    { M_IS_AGS_INITIALIZED, retFalse },
    { M_REQUEST_PURCHASE,   requestPurchase },
};

MethodsVoid methodsVoid[] = {
    { M_SAVE_SNAPSHOT,                 noop },
    { M_BEGIN_USER_INITIATED_SIGN_IN,  noop },
    { M_SIGN_OUT,                      noop },
    { M_SHOW_SIGN_IN_PROPOSE,          noop },
    { M_SHOW_SIGN_IN_PROPOSE_AT_FIRST, noop },
    { M_SHOW_ALERT_DIALOG,             showAlertDialog },
    { M_ON_LOADING_COMPLETED,          onLoadingCompleted },
    { M_SET_TUTORIAL_FILE,             setTutorialFile },
    { M_SHOW_TUTORIAL,                 showTutorial },
    { M_HIDE_TUTORIAL,                 noop },
    { M_SHOW_IN_APP_INFO,              noop },
    { M_HIDE_IN_APP_INFO,              noop },
    { M_SHOW_GALLERY,                  noop },
    { M_HIDE_GALLERY,                  noop },
    { M_PUT_FRAME_ON_PHOTO,            noop },
    { M_SHOW_SOCIAL_BUTTON,            noop },
    { M_HIDE_SOCIAL_BUTTON,            noop },
    { M_IS_IN_MENU_MODE,               noop },
    { M_IS_IN_GAME_MODE,               noop },
    { M_OPEN_ACHIEVEMENTS,             noop },
    { M_UNLOCK_ACHIEVEMENT,            unlockAchievement },
    { M_SYNC_PROGRESS,                 noop },
    { M_SYNCHRONIZE_GAME_DATA,         noop },
    { M_OPEN_MORE_GAMES,               noop },
    { M_OPEN_HOT_APP_BANNER,           noop },
    { M_OPEN_TAPJOY_OFFERS,            noop },
    { M_SEND_FLURRY_EVENT,             sendFlurryEvent },
    { M_SEND_GA_EVENT,                 noop },
    { M_SEND_GA_SCREEN,                noop },
    { M_LOG_IN,                        noop },
    { M_LOG_OUT,                       noop },
    { M_PUBLISH_FEED,                  noop },
    { M_SHOW_AD,                       noop },
    { M_IS_FYBER_ADS_AVALIABLE,        noop },
    { M_IS_REVIVE_AVAILABLE,           noop },
};

MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {};
MethodsLong methodsLong[] = {};
MethodsShort methodsShort[] = {};

/*
 * JNI Fields
 */

// System-wide constant that applications sometimes request
// https://developer.android.com/reference/android/content/Context.html#WINDOW_SERVICE
char WINDOW_SERVICE[] = "window";

// System-wide constant that's often used to determine Android version
// https://developer.android.com/reference/android/os/Build.VERSION.html#SDK_INT
const int SDK_INT = 19; // Android 4.4 / KitKat

// IceAgeAndroid.internet / IceAgeAndroid.purchaseManager: the engine only
// uses them as receivers for CallVoid/BooleanMethod, so any non-NULL
// placeholder works.
static int fake_internet_utils = 0x1A7E;
static int fake_purchase_manager = 0x9AC4;

// Field IDs start at 1: FalsoJNI returns id 0 for unknown field names, so no
// real field may use it.
NameToFieldID nameToFieldId[] = {
    { 1, "WINDOW_SERVICE",  FIELD_TYPE_OBJECT },
    { 2, "SDK_INT",         FIELD_TYPE_INT },
    { 3, "internet",        FIELD_TYPE_OBJECT },
    { 4, "purchaseManager", FIELD_TYPE_OBJECT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
    { 2, SDK_INT },
};
FieldsObject fieldsObject[] = {
    { 1, WINDOW_SERVICE },
    { 3, (jobject) &fake_internet_utils },
    { 4, (jobject) &fake_purchase_manager },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES
