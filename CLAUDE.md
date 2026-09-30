# Carnivores Ice Age — Port a PS Vita

Port de `Carnivores__Ice_Age_1.5.4_Android_4.0.apk` (Android) a PS Vita vía soloader. Generado con **psvita-port-toolkit**.

## Estructura

- `carnivoresiceage_extract/` — APK extraído (gitignored).
- `decompiled/` — Java (jadx) y pseudo-C (Ghidra) del/los .so (gitignored, regenerable).
- `source/`, `lib/so_util`, `lib/falso_jni` — scaffold del boilerplate (SoLoader + FalsoJNI).
- `PORTING_PLAN.md` — plan vivo, actualizar a medida que se confirman cosas del motor real.
- `port_progress.md` — bitácora, un bug confirmado a la vez.
- `.psvita-toolkit.json` — config para el toolkit standalone (build/deploy/logs/LiveArea/crash dumps).

Este port **no** tiene una copia local de `porting_tools/` -- todo el build/deploy/debug se maneja
desde **psvita-port-toolkit**, la herramienta standalone (fuera de este repo). Abrí el toolkit y
elegí "Continuar con un port existente" apuntando a esta carpeta.

## Hallazgos de motor (confirmados por análisis estático, 2026-09-30 — sin probar en consola)

- ABI armeabi. Carga 2 módulos: `libfmodex.so` @0x98000000 y `libIceAgeAndroid.so` @0x98400000.
  `libAmazonGamesJni.so` no se carga (el juego no importa nada de él).
- GLES 1.1 fijo (vitaGL). Texturas `.crt/.crthd`: RGB888/RGBA8888 crudo (header 5 bytes), PVRTC solo si bpp tiene el bit 0x80.
- Exports `Java_com_tatem_iceage_*` por nombre (no RegisterNatives). Ciclo de vida en `source/main.c`.
- Assets: el libzip interno del motor abre `ux0:data/carnivoresiceage/game.apk` directamente.
- Audio: FMOD en modo AUDIOTRACK → `source/reimpl/audio.c` emula `org.fmod.FMODAudioDevice`.
- Input: toques por cercanía (sin ID); botones → toques sobre `gui_controls[]` (`source/input.c`).
- **vitaGL vendorizada** en `vendor/vitaGL` (árbol de Zenonia4/DH2), compilada con `SOFTFP_ABI=1`: con el
  `libvitaGL.a` del SDK el juego corría a 60 FPS pero la pantalla quedaba negra (confirmado en consola).
  No usar `DRAW_SPEEDHACK`: el motor reescribe cada frame sus arrays de vértices en `.bss`.
- **Soft-float → VFP**: `source/reimpl/softfloat.c` engancha los helpers libgcc del `.so` (armeabi v5TE).
  Cualquier hook nuevo en 0xc1fd0..0xc3168 hay que revisarlo con objdump (trampolín de 8 bytes).
- Cámara del stick derecho: hook de `GUI_GetBackgroundMovements()` (`source/input.c`), no toques sintéticos.
- Detalle completo en `PORTING_PLAN.md`; bugs en `port_progress.md`.

## Flujo de trabajo esperado

1. Análisis de símbolos antes de tocar loader/source -- skill `psvita-port-init` cubrió la Fase 0-2.
2. Bootstrap del loader guiado por la skill `psvita-porting`.
3. Build/deploy con el toolkit standalone → probar en consola real.
4. Un bug a la vez, guiado por el log real -- skill `so-crash-triage`.
5. Actualizar `port_progress.md` con cada bug confirmado.
