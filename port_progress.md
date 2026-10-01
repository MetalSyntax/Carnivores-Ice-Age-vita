# Registro de Progreso — Carnivores Ice Age (PS Vita)

**Estado (2026-09-30):** loader completo, compila (`build/eboot.bin` 572 KB, `build/carnivoresiceage.vpk` 664 KB).
Todo lo que se puede hacer sin consola está hecho. **Nada probado en hardware real todavía.**

## Fase 1: Configuración y Preparación (Completada — 2026-09-24)
- Repo creado desde soloader-boilerplate, `.gitignore` anti-DMCA.
- APK `Carnivores__Ice_Age_1.5.4_Android_4.0.apk` copiado y extraído.
- ABI detectada: armeabi (elegida: armeabi).
- GLES: sin `glEsVersion` en el manifest; imports confirman GLES 1.1 fijo (`libGLESv1_CM.so`, `glVertexPointer`, `glTexEnvi`...).

## Fase 2: Decompilación (Completada — 2026-09-24)
- jadx: corrido (`decompiled/apk_jadx/sources`). (La entrada anterior decía "NO corrido": era incorrecta.)
- Ghidra (.so): corrido para cada .so.

## Fase 3: Análisis del Motor Real (Completada — 2026-09-30)
- [x] Motor Tatem compartido con Carnivores Dinosaur Hunter (export residual `DinHunterAndroid_getFreePoints`),
      pero clases/firmas distintas: todo se re-verificó en este `.so`.
- [x] Ciclo de vida nativo leído de `IceAgeAndroid.java`/`IceAgeRenderer.java`/`IceAgeGLSurface.java` + pseudo-C (ver PORTING_PLAN §2-3).
- [x] Exports JNI reales por convención `Java_*` (no hay `RegisterNatives`). El "no se encontraron exports"
      del plan autogenerado era falso: hay 42 en `libIceAgeAndroid.so` (+2 `_Z…Java_*` C++ mangled) y 2 en `libfmodex.so`.
- [x] Assets: libzip estático abre el APK directamente (`zip_fopen` con `ZIP_FL_NODIR`), OBB como fallback.
- [x] Audio: `FMOD_OUTPUTTYPE_AUDIOTRACK` (0x15) → requiere emular `org.fmod.FMODAudioDevice`.
- [x] Input: toques en píxeles de superficie, re-identificados por cercanía (sin ID).
- [x] `libAmazonGamesJni.so` no aporta ningún símbolo al juego → no se carga.

## Fase 4: Bootstrap del Loader (Completada — 2026-09-30)
- `source/utils/init.c`: carga `libfmodex.so` @0x98000000 y `libIceAgeAndroid.so` @0x98400000 (memsz verificados, sin solape).
- `CMakeLists.txt`: `SO_PATH=libIceAgeAndroid.so`, `FMOD_SO_PATH`, `APK_PATH=game.apk`; `input.c`, `audio.c` añadidos.
  **Ojo:** el `CMakeCache.txt` viejo tenía `SO_PATH=main.so` cacheado; se borró el cache para reconfigurar.
- Imports: 0 sin resolver (script `nm -D` vs `dynlib.c` + exports de fmodex).

### Bugs corregidos en esta fase (encontrados por análisis estático, no en consola)
1. **Link roto:** `dynlib.c` definía `void *__dso_handle = NULL;` → `multiple definition` contra `crtbegin.o`.
   Fix: `extern` (lo provee crtbegin). El build de la otra IA (01:07) era anterior a ese cambio (01:24).
2. **Crash seguro al abrir el APK:** con `USE_SCELIBC_IO`, `fopen` devuelve un `FILE*` de SceLibc pero
   `fseeko`/`ftello`/`clearerr`/`rewind` estaban mapeados a newlib. El libzip del motor usa `fseeko/ftello`
   en `_zip_find_central_dir` (pseudo-C l.76845-77212). Fix: wrappers `*_soloader` sobre `sceLibcBridge_fseek/ftell`.
3. **Crash en cualquier warning de libpng:** libpng hace `fprintf(stderr, ...)`; `stderr` bionic = `&__sF[2]`
   = copia de un `FILE` newlib en `__sF_fake`, que iba a `sceLibcBridge_fprintf`. Fix: wrappers
   `fprintf/vfprintf/fputc/fputs/fwrite/fflush` que detectan `__sF_fake` y lo mandan al log.
4. **`sysconf` → 0:** STLport guarda `sysconf(_SC_PAGESIZE)` (0x27) en `_Filebuf_base::_M_page_size`
   (l.81361) y luego alinea/divide. Fix: `sysconf_soloader` (4096 para page size, 1 CPU).
5. **FMOD podría ir a OpenSL:** `dlopen` devolvía 1 para todo. Fix: `dlopen_soloader` devuelve NULL para `libOpenSLES.so`.

## Fase 5: FalsoJNI (Completada — 2026-09-30)
- `source/java.c`: 41 métodos que el `.so` realmente pide (`GetMethodID` con literales), no los 500+ stubs
  autogenerados (`generated_jni_*.{c,h}` quedan sin compilar como referencia).
- `getCurrentLanguage` → idioma Vita (en/de/fr/es) o `language` en `config.txt`. **Nunca NULL**: `Locale_Init` hace `strlen`.
- `getSnapshot` → `"{}"` (JsonBox lo parsea sin null-check).
- Field IDs empiezan en 1: FalsoJNI devuelve id 0 para campos desconocidos.
- `lib/falso_jni/FalsoJNI.c`: `GetDirectBufferAddress` devuelve el propio buffer (para `fmodProcess`).
- `lib/falso_jni/FalsoJNI_Logger.c`: salida redirigida al log a archivo.

## Fase 6: Gráficos (Completada — sin probar)
- vitaGL 960x544 MSAA 4x (boilerplate). `createFramebuffer` → `scaleX=0.5`, `scaleY=320/544` (el juego estira su espacio lógico 480x320).
- Texturas PVRTC 4bpp (0x8C00/0x8C02) soportadas por vitaGL.

## Fase 7: Audio (Completada — sin probar)
- `source/reimpl/audio.c`: réplica de `FMODAudioDevice.run()` → `sceAudioOutOpenPort(BGM, grain, rate)`.
- Arranca antes de `DidFinishLaunching` (como en Android) y espera a que `fmodGetInfo(SAMPLERATE) > 0`.

## Fase 8: Input (Completada — sin probar)
- `source/input.c`: táctil frontal con slots; botones/sticks → toques sintéticos sobre `gui_controls[]` (ver PORTING_PLAN §7).

## Fase 9: Logs (Completada)
- `source/utils/logger.c`: `ux0:data/carnivoresiceage/logs/carnivoresiceage_NNN.log` + `next.idx`,
  sin buffer durante el arranque, con buffer en el bucle. `psvita-toolkit log-standard --fix-dirs`: OK.
- `l_info/l_warn/l_success` ahora siempre activos (antes solo con `DEBUG_SOLOADER`, el Release no logueaba nada).

## Fase 10: LiveArea / VPK (Completada)
- `extras/scripts/make_livearea.py` genera icon0/pic0/bg0/startup desde `res/` del APK (8-bit indexado, tamaños exactos).
- VPK sin archivos `._*`.

## Fase 11: Pruebas en hardware real (PENDIENTE — requiere consola)

### Instalación
1. Plugins: `kubridge.skprx` y `libshacccg.suprx` (ur0:data) instalados.
2. Instalar `build/carnivoresiceage.vpk` (`psvita-toolkit deploy --vpk` → VitaShell).
3. Copiar el contenido de `ux0_data/carnivoresiceage/` (ya preparado, gitignored) a `ux0:data/carnivoresiceage/`:
   - `game.apk` (el APK original renombrado)
   - `libIceAgeAndroid.so`, `libfmodex.so` (de `lib/armeabi/` del APK)
   - opcional: `main.obb` si algún asset falta.
4. Lanzar. Log en `ux0:data/carnivoresiceage/logs/carnivoresiceage_001.log`.

### Qué mirar en el primer log
- `libfmodex.so initialized` / `libIceAgeAndroid.so initialized` → loader OK.
- `[ALOG] ... Archive ... opened. Opening file ...` → libzip lee el APK.
- `audio: FMOD AudioTrack output up: rate=...` → audio OK.
- `nativeApplicationDidFinishLaunching returned.` → init del juego OK.
- Si crashea: `psvita-toolkit analyze <.psp2dmp>` y skill `so-crash-triage`, un bug a la vez.

### Bitácora de pruebas
_(vacía — anotar aquí cada bug confirmado en consola)_

#### Prueba 1 (log `carnivoresiceage_001.log`) — pantalla negra, 0 FPS, audio OK
- Todo el init OK: loader, FMOD AudioTrack (24 kHz), libzip lee `game.apk`, texturas/fuentes/modelos/sonidos cargan.
- La carga entera (~31 s) ocurre dentro del **primer** `layoutSubviews()` → `mainLoop()` → `Process()`;
  pantalla negra durante ese tiempo es esperable (en Android lo tapa el splash Java).
- `JNI: onLoadingCompleted()` → `MenuProcess()` → `showSocialButton()`; el único `GA screen sended` sale de
  `Render()` (`SendGAScreen("Main")` en su primera llamada) → el frame 0 llegó a `Render()`. Después el log calla.
- Descartado por análisis estático: ABI de floats (el `.so` es softfp v5TE/VFPv2, igual que el build y vitaGL),
  imports GL (40, todos directos a vitaGL), `v_sx/v_sy/hd_mode` (los pone `EAGLView::EAGLView()` desde `.init_array`),
  `purple_screen_fix=0`, `device_orientation=0` igual que en Android.
- **Pendiente de confirmar en consola:** `main.c` ahora traza los frames 0-5 (tiempos de `layoutSubviews`/`gl_swap`,
  `glGetError`), memoria libre vitaGL/kernel, `fps` siempre cada 5 s y un hilo watchdog que loguea en qué etapa
  se queda el bucle si el contador de frames deja de avanzar.

#### Prueba 2 (log `carnivoresiceage_002.log`) — sigue negro, pero el bucle corre a 60 FPS
- Watchdog: el hilo principal está dentro del frame 0 (carga) ~31 s y **después no se cuelga**: `fps: 60`.
- Frames 1-5: `layoutSubviews` ~0,4 ms, `gl_swap` 0,1-7 ms, `glGetError` 0. VRAM vitaGL 78 MB → 39 MB tras cargar.
- Descartado: fade infinito (`Process()` fuerza `delta` a 1/60 si ≤ 0). Texturas del menú = `.crthd` RGB888/RGBA8888
  crudo (header 5 bytes: w16, h16, bpp) → `glTexImage2D`; PVRTC solo si bpp tiene el bit 0x80.
- **Siguiente prueba:** `RENDER_PROBE` en `main.c` (fondo magenta + cuadrado verde propio) para separar
  "vitaGL no presenta" / "el motor no dibuja" / "el motor dibuja en negro".

#### Prueba 3 (log `carnivoresiceage_003.log`) — todo negro, ni el quad de prueba
- `RENDER_PROBE` (fondo magenta + quad verde propio): **nada visible**, 60 FPS, `glGetError` 0 → la presentación
  de vitaGL está muerta; el motor no tiene la culpa.
- Mismo síntoma y causa que Zenonia3/4 (Z4 `port_progress.md` Fase 124): el `libvitaGL.a` precompilado del SDK
  no está compilado con `SOFTFP_ABI=1` → sin `HAVE_SOFTFP_ABI` vitaGL toma caminos incompatibles con un loader softfp.
- **Fix aplicado (pendiente de probar en consola):** vitaGL vendorizada en `vendor/vitaGL` (copia del árbol de
  Zenonia4-vita = DH2, upstream cd3791e + defines `SCE_GXM_*` + `#if 0` en la autodetección de `system_app_mode`
  en `source/gxm.c`). `CMakeLists.txt` la compila con `SOFTFP_ABI=1 NO_SPLASHSCREEN=1` (verificado:
  `-mfloat-abi=softfp -DHAVE_SOFTFP_ABI -DSKIP_SPLASHSCREEN`) y enlaza contra ella en vez de `-lvitaGL`.
  Sin `DRAW_SPEEDHACK`: el motor reescribe cada frame sus arrays de vértices en `.bss`.

#### Prueba 4 (log `carnivoresiceage_004.log`) — **se ve: menú y gameplay OK** (fix vitaGL vendorizada confirmado)
- Menú a 60 FPS; gameplay 3D a **16-19 FPS**. Carga inicial ~31 s.
- El motor loguea en cada frame (`pack1_purchased` en el menú, `isReviveAvailable` en el juego) y marca como
  ERROR mensajes normales (`SOUUNDS 5`, `Loadings steps`) → cada uno fuerza un flush del log a la tarjeta.
- Quitada toda la instrumentación de las pruebas 1-3 (watchdog, trazas por frame, memoria, `RENDER_PROBE`).

### Performance y cámara (2026-09-30, pendiente de medir en consola)
1. **Soft-float → VFP** (`source/reimpl/softfloat.c`, `vfp_float 1`): el `.so` es armeabi v5TE y todo su float/double
   pasa por libgcc ieee754 enlazado estático en 0xc1fd0..0xc3168. Se enganchan 32 entradas (fadd/fmul/fdiv,
   conversiones, fcmp*, gesf2/lesf2/cmpsf2 y equivalentes double) con trampolín ARM de 8 bytes.
   Verificado con objdump: no hay entradas a <8 bytes entre sí; los únicos saltos dentro de una ventana de
   8 bytes salen de `__gesf2/__lesf2/__gedf2/__ledf2`, también enganchados. `fsub/frsub/dsub/drsub` NO se
   enganchan (invierten signo y caen en fadd/dadd). `__aeabi_cf*cmp*` (flags en CPSR) intactos.
   `softfloat.c` compila con `-fno-fast-math`; el objeto es VFP puro (sin llamadas).
2. **`engine_log 0`** (por defecto): `__android_log_*` sale antes de formatear (salvo FATAL).
3. **`msaa 1`** (2x, antes 4x fijo); `0` = sin MSAA.
4. **Cámara con stick derecho**: en vez de arrastres sintéticos (8 px/frame → dependía de los FPS y el
   re-anclaje cada 250 ms la topaba en ~480 px/s), hook de `GUI_GetBackgroundMovements()` (reimplementada 1:1
   del pseudo-C; único llamador `Game_ProcessPlayerControls`) que suma el stick: 360 px lógicos/s a
   deflexión máxima con `look_sensitivity 100`, curva cuadrática, independiente de los FPS.
5. `show_fps 1` ahora reporta además ms de motor (CPU) vs `gl_swap` (GPU/vblank) por frame.
- Pendiente de investigar: por cada asset el motor intenta abrir los "bundles" (`Failed to open archive`
  x2 por archivo) antes del APK principal; posible costo en la carga de 31 s.

#### Prueba 5 (log `carnivoresiceage_005.log`) — congelamiento al pulsar Cuadrado; carga sigue en ~30 s
- **Congelamiento (causa confirmada por análisis estático):** Cuadrado = `game_weapon` → `Weapon_TakeWeapon()` →
  `Weapons_Animate()` hace `do t -= len; while (len <= t);` con `len = (frames-1)/kps` del `.ani`
  (`CharacterInfo_Load`, `characters_info + slot*0xf7c + 0x2c + i*0x34 + 0x58`). Si falta el `.ani`, `len = 0`
  → bucle infinito. La partida usaba SHOTGUN: `shotgun.can` está en el APK pero `shotgun.3dn` y
  `shotgun_animation_*.ani` no están en ningún archivo de Ice Age (solo en el APK de Dinosaur Hunter).
  `dbsgun`/`x_bow` (+ área 6) están en `CarnivoresBundleTwo.apk`; `sniper` (+ áreas 3-4) en `CarnivoresBundleOne.apk`.
  - Fix: hook de `CharacterInfo_Load` (ARM, 0x5f1d0): si un slot de arma (0-5, 0x22) queda sin animaciones
    válidas se recarga en el mismo slot con `dbsgun` → `rifle` → `pistol` (stats por slot intactas). Para
    no-armas: longitudes 0 → 1.0 y warning.
  - `main.c` pasa los bundles reales: `main.obb` (ambos), o `bundle1.apk`/`CarnivoresBundleOne.apk` y
    `bundle2.apk`/`CarnivoresBundleTwo.apk`.
- **Carga:** el log 004 muestra 300-600 ms por cada `.ogg` y 6,5 s tras `menumusic_cmpr.ogg` → FMOD decodificando
  Vorbis a PCM con soft-float (`libfmodex.so` también es armeabi v5TE; el parche VFP solo cubría el motor).
  - Fix: `softfloat_patch(mod)` se aplica a **ambos** módulos desde `load_module()`: hooks de entrada para los
    helpers enlazados (fmod: 15 float en 0xd03ac..0xd0a4c, escaneo de saltos OK) + override de GOT para los
    importados (fmod importa los double; antes iban a la libgcc soft del vitasdk).
  - Caché de `zip_open`/`zip_close` (Thumb, vía PLT; escaneo OK): el motor abría y cerraba los bundles en cada
    búsqueda fallida (todas las texturas prueban `.crthd` → `.tga` → `.crt`). Ahora quedan abiertos toda la
    sesión y un bundle ausente responde "no disponible" sin tocar la tarjeta.

#### Prueba 6 (log `carnivoresiceage_006.log`) — **funciona en consola**: Cuadrado ya no congela; carga 30,6 s → 16,6 s
- Confirmado en consola: partida completa jugable, arma elegible sin congelamiento, carga ~17 s.
- `softfloat`: fmod 16 hooks + 10 imports, motor 32 hooks. Carga: `Entering main loop` 3,4 s → `onLoadingCompleted`
  20,0 s (antes 3,4 → 34,0 s).
- Arma DOUBLE-BARRELED SHOTGUN elegida: `assets: weapon 'dbsgun' has no model/animations, using 'rifle'` →
  el fallback funciona, pero reveló que **los bundles no abrían**: `zip: CarnivoresBundleOne.apk not available`
  aunque el archivo existe (y antes, `game.apk` como bundle fallaba igual).
  - Causa: libzip está compilado contra bionic y `ferror(fp)` quedó **inline**: `ldrh r3,[fp,#12]` & `0x40`
    (`fp->_flags & __SERR`). Nuestros `FILE*` son de SceLibc (otro layout): el bit dio 0 para el APK por
    casualidad y 1 para los bundles → `ZIP_ER_READ`.
  - Fix: `patch.c` reemplaza los 5 `ldrh r3,[r3,#12]` (0x899b → `movs r3,#0` 0x2300) en `_zip_find_central_dir`,
    `_zip_readcdir` (x2), `_zip_cdir_write`, `_zip_dirent_write`, verificando el opcode antes. **Pendiente de
    probar en consola.**
- Las advertencias de `hunter2`/`sship1`/`diatr` eran falsas: tienen animaciones de 1 frame (duración legítima 0).
  Ahora "faltante" = sin frames/datos; toda duración 0 pasa a 0,01 s (termina en un frame, como 0, sin colgarse).

#### Prueba 7 (log `carnivoresiceage_007.log`) — **packs confirmados**: el parche de `ferror` funciona
- `zip: ...CarnivoresBundleOne.apk opened (kept open)` y `...BundleTwo.apk opened`; ya no aparece el fallback de
  `dbsgun`. Quedan solo las advertencias benignas de `hunter2`/`sship1`/`diatr`.
- Pedido de controles: Círculo pasa a ser **llamar** (junto a D-pad arriba); antes duplicaba Start (atrás/pausa).
- Botones táctiles al 1 % de opacidad (`hud_opacity`, default 1): hook de `GUI_DrawControls` (ARM, 0x46e54) que
  escala el alfa del color ARGB (`+0x28`) de movement/fire/alt_fire/weapon/binoculars/call/photomode solo durante
  el dibujado (el motor anima el alfa de `game_fire` cada frame, por eso no se escribe una vez). La brújula es
  `Navigations_Render()`, fuera de `gui_controls[]`: no se toca. Listas de armas/llamadas quedan visibles.
  **Pendiente de probar en consola.**

#### Prueba 10 (log `carnivoresiceage_010.log`) — navegación de menús rompía el juego al desenfundar
- Síntoma: tras empezar la caza (log llega a `weapon 'shotgun' ... using 'dbsgun'`), pantalla morada con letras
  arriba y controles rotos. El log no muestra crash.
- Causa (pseudo-C): el modo menú se activaba cuando `game_movement_controller` no era usable, pero al desenfundar el
  HUD pasa a subgrupo `0x800` (fire/alt_fire = `0x801`, movement = `0x4025` → no incluye `0x800`). El juego quedaba
  en "modo menú": botones de juego muertos, X/Círculo = toque/atrás, y el foco escribía `+0x34` (estado *tocado* que
  `GUI_TouchesEnded` convierte en latch `+0x35`/`GUI_ControlIsPressed`) en un control del HUD cada frame.
- Fix (`input.c`): gameplay = movement/fire/weapon/photo_shot usable. Nunca se escribe `+0x34`; el foco se dibuja como
  marco GL tras `GUI_DrawControls` (las celdas del menú de caza se dibujan con `Menu_Draw*Button`, sin sprite de
  "pressed": por eso antes no se veía nada ahí). Solo controles con centro en pantalla (las páginas de caza se
  deslizan). Sliders son tipo **1** (no 2; 2 = joystick) con rango `+0x178/+0x17C` (no 0..1).
- Facebook: se quitaron los hooks de `GUI_SetControlVisible/Active` (`patch.c`): mientras `Menu_Init` no creaba los
  botones de Facebook su ID era 0 = `game_movement_controller`. Se ocultan desde `input.c` solo con ID > 0 y se limpia
  su latch `+0x35`. `java.c`: firmas corregidas (`showAlertDialog(int)`, `unlockAchievement(int)`, eran string).
  **Pendiente de probar en consola.**
- Indicador de foco en menús: rojo "sostenido" del motor (`+0x34 = 1` solo durante `GUI_DrawControls`, valor original
  restaurado después) + cuadro amarillo como en el port de Dinosaur Hunter (relleno `0x2800c8ff`, `0x50` con X
  apretado, borde 1,5 px `0xff00c8ff`) ajustado al rect de cada control.
- Prueba 13 (log `carnivoresiceage_013.log`, sin errores): el cuadro no se veía. `GUI_DrawControls()` solo encola
  sprites; `Render()` los dibuja al final con `Sprites_Render()` → `Font_Render()` → `GUI_RenderFade()`, así que el fondo
  del menú tapaba el cuadro. Ahora se dibuja desde un hook de `Font_Render()` (ARM 0x438fc, como en Dinosaur Hunter),
  en la proyección GUI del motor. **Pendiente de probar en consola.**
- Prueba 14 (log `carnivoresiceage_014.log`): el cuadro amarillo se ve en el menú ✅. Volvió el morado en el juego,
  y **no era input**: todas las pruebas buenas (004-009) fueron en la zona 0, y las dos moradas (010, 014) son las
  únicas en la zona 1. La zona 1 carga `area2` (`Game_StartLoading("area2")`), que no está en el APK (solo `area1`)
  ni en los `CarnivoresBundleOne/Two.apk`. Esos bundles tienen el **mismo MD5 que los de Dinosaur Hunter** (contienen
  `area3/4/6` de ese juego). Ice Age guarda las zonas 2-5 en su `main.obb` (un archivo para los dos packs). Como
  `unlock_bundles=1` desbloqueaba `pack1_purchased` igual, se cargaba un terreno vacío. Fix (`main.c`): solo se
  desbloquea si algún bundle contiene `area2.rsn` (se busca en la cola del zip, donde está el directorio central).
  Si no, aparece `Content packs not unlocked` en el log y las zonas quedan con candado. README/RELEASE_NOTES corregidos.

### Release v1.0
- Build limpio verificado 2026-09-30 (`carnivoresiceage.vpk`, VITA_VERSION 01.00), incluye el parche de `ferror`.
- Procedimiento de publicación: `RELEASE.md`. Único pendiente antes de publicar: confirmar en consola que los
  packs abren (log sin `zip: ... not available`; ver checklist).
- `README.md` reescrito para GitHub (requisitos, instalación con tabla de archivos, packs, controles, opciones,
  problemas conocidos, reporte de bugs, build, créditos, licencias). `RELEASE_NOTES.md` = texto de la release.
