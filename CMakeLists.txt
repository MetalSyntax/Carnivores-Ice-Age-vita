cmake_minimum_required(VERSION 3.14)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)


if (NOT DEFINED ENV{VITASDK} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "Please define VITASDK env var to point to your SDK path or set CMAKE_TOOLCHAIN_FILE.")
endif ()

set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Toolchain file")

project(carnivoresiceage C CXX)

include("${VITASDK}/share/vita.cmake" REQUIRED)

# Vita application configuration
set(VITA_APP_NAME "Carnivores Ice Age")
set(VITA_TITLEID "PSVCSIA01")
set(VITA_VPKNAME "carnivoresiceage")
set(VITA_VERSION "01.00")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

# Optional: used for "send" and "dump" targets
set(PSVITAIP "192.168.3.15" CACHE STRING "PSVita IP (for FTP access)")
set(PARSECOREPATH "$ENV{PARSECOREPATH}" CACHE STRING "vita-parse-core main.py")

# Game-specific definitions
set(DATA_PATH "ux0:data/carnivoresiceage/" CACHE STRING "Path to data (with trailing /)")
set(SO_PATH "${DATA_PATH}libIceAgeAndroid.so" CACHE STRING "Path to the game .so")
set(FMOD_SO_PATH "${DATA_PATH}libfmodex.so" CACHE STRING "Path to libfmodex.so")
set(APK_PATH "${DATA_PATH}game.apk" CACHE STRING "Path to the original APK (assets are read from it via libzip)")
add_definitions(-DDATA_PATH="${DATA_PATH}" -DSO_PATH="${SO_PATH}" -DFMOD_SO_PATH="${FMOD_SO_PATH}" -DAPK_PATH="${APK_PATH}")

option(USE_SCELIBC_IO "Use SceLibcBridge for IO and some other std functions" ON)
if (USE_SCELIBC_IO)
  add_definitions(-DUSE_SCELIBC_IO)
endif()

set(SHADER_FORMAT "GLSL" CACHE STRING "Preferred shader format (one of 'GLSL', 'CG', 'GXP')")
if (${SHADER_FORMAT} STREQUAL "GLSL")
  add_definitions(-DUSE_GLSL_SHADERS)
elseif (${SHADER_FORMAT} STREQUAL "CG")
  add_definitions(-DUSE_CG_SHADERS)
elseif (${SHADER_FORMAT} STREQUAL "GXP")
  add_definitions(-DUSE_GXP_SHADERS)
else()
  message(FATAL_ERROR "SHADER_FORMAT option can only be one of: 'GLSL', 'CG', 'GXP'")
endif()

set(valid_dump_formats GLSL CG)
option(DUMP_COMPILED_SHADERS "When using GLSL or CG shader format, cache compiled shaders on disk" ON)
if (DUMP_COMPILED_SHADERS)
  if (SHADER_FORMAT IN_LIST valid_dump_formats)
    add_definitions(-DDUMP_COMPILED_SHADERS)
  else()
    message(WARNING "DUMP_COMPILED_SHADERS has no effect for the chosen shader format ${SHADER_FORMAT}")
  endif()
endif()

# Unique debug definition for loader's logging to not interfere with anything
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG_SOLOADER)
  # Uncomment this for verbose FalsoJNI logging:
  #add_definitions(-DFALSOJNI_DEBUGLEVEL=0)
endif()

# makes sincos, sincosf, etc. visible
add_definitions(-D_GNU_SOURCE -D__POSIX_VISIBLE=999999)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3 -g -ffast-math -mfloat-abi=softfp -Wno-deprecated")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=gnu++20 -Wno-write-strings -Wno-psabi")

add_executable(${CMAKE_PROJECT_NAME}
               source/main.c
               source/input.c
               source/dynlib.c
               source/java.c
               source/patch.c
               source/reimpl/bits/_ctype.c
               source/reimpl/errno.c
               source/reimpl/io.c
               source/reimpl/log.c
               source/reimpl/mem.c
               source/reimpl/pthr.c
               source/reimpl/sys.c
               source/reimpl/time64.c
               source/reimpl/asset_manager.cpp
               source/reimpl/audio.c
               source/reimpl/softfloat.c
               source/utils/dialog.c
               source/utils/glutil.c
               source/utils/init.c
               source/utils/logger.c
               source/utils/settings.c
               source/utils/utils.c
               lib/falso_jni/FalsoJNI.c
               lib/falso_jni/FalsoJNI_ImplBridge.c
               lib/falso_jni/FalsoJNI_Logger.c
               lib/falso_jni/converter.c
               lib/sha1/sha1.c
               lib/fios/fios.c
               lib/so_util/so_util.c)

## vitaGL vendorizada en vendor/vitaGL (copia del arbol de Zenonia4-vita, que a su vez es el de
## Dungeon-Hunter-2-vita: upstream cd3791e + defines SCE_GXM_* para este vitasdk + autodeteccion
## de system_app_mode deshabilitada con #if 0 en source/gxm.c). Ver vendor/vitaGL/VENDORED.md.
##
## Por que: con el libvitaGL.a precompilado del SDK el juego corre a 60 FPS, glGetError()=0,
## pero NADA llega al display -- ni siquiera un quad de prueba propio (RENDER_PROBE, log 003).
## Mismo sintoma y misma causa que Zenonia3/4 (Z4 port_progress Fase 124): sin SOFTFP_ABI=1
## vitaGL no define HAVE_SOFTFP_ABI y toma caminos incompatibles con este loader softfp.
##
## Flags: solo lo imprescindible. NO usar DRAW_SPEEDHACK: el motor reescribe cada frame sus
## arrays de vertices en .bss (Sprites_Render) y vitaGL tiene que copiarlos.
set(VITAGL_DIR ${CMAKE_SOURCE_DIR}/vendor/vitaGL)
set(VITAGL_MAKE_FLAGS_STR "SOFTFP_ABI=1 NO_SPLASHSCREEN=1")
set(VITAGL_EXTRA_FLAGS "" CACHE STRING "Extra vitaGL make flags (space-separated)")
## separate_arguments es OBLIGATORIO: sin esto make recibe todos los flags como un unico
## argumento y los ignora en silencio (trampa confirmada en Zenonia4).
separate_arguments(VITAGL_MAKE_FLAGS UNIX_COMMAND "${VITAGL_MAKE_FLAGS_STR} ${VITAGL_EXTRA_FLAGS}")
set(VITAGL_STAMP "${CMAKE_BINARY_DIR}/vitagl.stamp")
## custom_target (no ExternalProject) para que se reevalue en cada build: build_vitagl.sh
## hace `make clean` solo si los flags cambiaron; el make de abajo es incremental.
add_custom_target(vitaGL_lib
  COMMAND bash "${VITAGL_DIR}/build_vitagl.sh" "${VITAGL_MAKE_FLAGS_STR} ${VITAGL_EXTRA_FLAGS}" "${VITAGL_STAMP}" "${VITAGL_DIR}"
  COMMAND make -C "${VITAGL_DIR}" ${VITAGL_MAKE_FLAGS}
  BYPRODUCTS "${VITAGL_DIR}/libvitaGL.a"
  COMMENT "Building vendored vitaGL with ${VITAGL_MAKE_FLAGS_STR} ${VITAGL_EXTRA_FLAGS}"
)
add_dependencies(${CMAKE_PROJECT_NAME} vitaGL_lib)
## Antes que el vitaGL.h del sysroot.
target_include_directories(${CMAKE_PROJECT_NAME} BEFORE PUBLIC ${VITAGL_DIR}/source)

## softfloat.c reimplementa los helpers soft-float de libgcc: sin -ffast-math,
## las comparaciones con NaN tienen que dar lo mismo que en libgcc.
set_source_files_properties(source/reimpl/softfloat.c PROPERTIES COMPILE_OPTIONS "-fno-fast-math")

add_subdirectory(lib/libc_bridge)
add_dependencies(${CMAKE_PROJECT_NAME} SceLibcBridge)

target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM
                           PUBLIC ${CMAKE_SOURCE_DIR}/lib
                           )

target_include_directories(${CMAKE_PROJECT_NAME}
                           PUBLIC ${CMAKE_SOURCE_DIR}/source
                           )

target_link_libraries(${CMAKE_PROJECT_NAME}
                      -Wl,--whole-archive pthread -Wl,--no-whole-archive
                      "${VITAGL_DIR}/libvitaGL.a"
                      stdc++
                      vitashark
                      SceShaccCgExt
                      taihen_stub
                      SceShaccCg_stub
                      z
                      m
                      ScePgf_stub
                      mathneon
                      kubridge_stub
                      ${CMAKE_BINARY_DIR}/lib/libc_bridge/stubs/libSceLibcBridge_stub.a
                      SceHid_stub
                      SceTouch_stub
                      SceMotion_stub
                      SceAppMgr_stub
                      SceAppUtil_stub
                      SceAudio_stub
                      SceAudioIn_stub
                      SceCommonDialog_stub
                      SceDisplay_stub
                      SceKernelDmacMgr_stub
                      SceFios2_stub
                      SceCtrl_stub
                      SceGxm_stub
                      ScePower_stub
                      SceTouch_stub
                      SceVshBridge_stub
                      )

vita_create_self(eboot.bin ${CMAKE_PROJECT_NAME} UNSAFE NOASLR)
vita_create_vpk(${VITA_VPKNAME}.vpk ${VITA_TITLEID} eboot.bin
                VERSION ${VITA_VERSION}
                NAME ${VITA_APP_NAME}
                FILE ${CMAKE_SOURCE_DIR}/extras/cpuinfo cpuinfo
                ${CMAKE_SOURCE_DIR}/extras/meminfo meminfo
                ${CMAKE_SOURCE_DIR}/extras/livearea/icon0.png sce_sys/icon0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/pic0.png sce_sys/pic0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/startup.png sce_sys/livearea/contents/startup.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/bg0.png sce_sys/livearea/contents/bg0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/template.xml sce_sys/livearea/contents/template.xml
                )

add_custom_target(send
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND curl -T eboot.bin ftp://${PSVITAIP}:1337/ux0:/app/${VITA_TITLEID}/
                  COMMAND echo launch ${VITA_TITLEID} | nc ${PSVITAIP} 1338
                  DEPENDS ${VITA_VPKNAME}.vpk-vpk
                  )

add_custom_target(send_kvdb
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND curl -T eboot.bin ftp://${PSVITAIP}:1337/ux0:/app/${VITA_TITLEID}/
                  COMMAND echo launch ux0:/app/${VITA_TITLEID}/eboot.bin | nc ${PSVITAIP} 1338
                  DEPENDS ${VITA_VPKNAME}.vpk-vpk
                  )

add_custom_target(dump
                  COMMAND ${CMAKE_SOURCE_DIR}/extras/scripts/get_dump.sh ${PSVITAIP}
                  COMMAND python3 ${PARSECOREPATH} coredump ${CMAKE_PROJECT_NAME}
                  )

add_custom_target(reboot
                  COMMAND echo reboot | nc ${PSVITAIP} 1338
                  )
