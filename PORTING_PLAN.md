# Plan de Port — Carnivores Ice Age (PS Vita)

> Generado por psvita-port-toolkit el 2026-09-24. Reescrito el 2026-09-30 con el análisis real del
> motor (objdump/nm + Ghidra + jadx). Todo lo marcado **[confirmado estático]** se verificó contra
> el binario/pseudo-C; **nada** está probado todavía en consola real.

## 0. Contexto

- **Juego:** Carnivores Ice Age 1.5.4 (Tatem Games / Action Forms)
- **Paquete Java:** `com.tatemgames.iceage` (clases en `com.tatem.iceage.*`)
- **APK original:** `Carnivores__Ice_Age_1.5.4_Android_4.0.apk`
- **TITLEID:** `PSVCSIA01`
- **Motor:** propietario Tatem ("SD" engine, porte de iOS: `EAGLView`, `CGPoint`, lógica 480x320).
  Mismo motor que `Carnivores-Dinosaur-Hunter-vita` (el `.so` incluso exporta
  `Java_com_tatem_dinhunter_DinHunterAndroid_getFreePoints`), pero con nombres de clase y
  firmas distintas -- no se copió código de ese port sin re-verificarlo aquí (ese port tampoco
  está probado en hardware).

## 1. Arquitectura / binarios [confirmado estático]

| .so | Tamaño | Rol | Se carga |
|---|---|---|---|
| `libfmodex.so` | 888 KB (memsz 0x11BDE0) | FMOD Ex 4.x | Sí, en `0x98000000` |
| `libIceAgeAndroid.so` | 938 KB (memsz ~0x1620000, 22 MB de .bss) | Juego | Sí, en `0x98400000` |
| `libAmazonGamesJni.so` | 277 KB | Puente Amazon GameCircle | **No**: `nm -D` muestra que el juego no importa ningún símbolo suyo |

- ABI `armeabi` (ARMv5TE/v6 soft-float). Vita lo ejecuta tal cual; el loader compila con `-mfloat-abi=softfp`,
  por lo que los `jfloat` de `touchesBegan(x, y)` viajan en `r2/r3` igual que en Android.
- `DT_NEEDED` del juego: `libfmodex.so libAmazonGamesJni.so libGLESv1_CM.so liblog.so libz.so libm.so libstdc++.so libc.so libdl.so`.
- Imports: 0 sin resolver contra `source/dynlib.c` + exports de `libfmodex.so` (script de comparación con `nm -D`).
- `psvita-toolkit align-check`: sin `ldrd/strd/vld1/vldm`, sin riesgos de alineación.

## 2. Exports JNI usados por el loader [confirmado estático]

Convención de nombres (`Java_*`), sin `RegisterNatives`. `JNI_OnLoad` solo guarda `jvm` y devuelve `JNI_VERSION_1_6`;
todas las funciones obtienen el env con `jvm->GetEnv()`.

| Export | Qué hace (pseudo-C) |
|---|---|
| `IceAgeAndroid_nativeSetBundlesPaths(obb, obb)` | guarda `bundle1Path/bundle2Path` (archivos zip de expansión) |
| `IceAgeAndroid_nativeApplicationDidFinishLaunching(extStorage, apk, filesDir)` | `basedir=filesDir`, `photos_dir=<ext>/.iceage/photos`, `apkPath`, `activity=NewGlobalRef(thiz)`, `InitGame/InitGameData/LoadGameData`, `Locale_Init` |
| `IceAgeRenderer_createFramebuffer(w, h)` | `TexManager_ReloadAllTextures` + `initGL`, `scaleX=480/w`, `scaleY=320/h` |
| `IceAgeRenderer_nativeResize(w, h)` | `real_width/real_height` |
| `IceAgeRenderer_layoutSubviews()` | `clearGL(); mainLoop();` (Process + Render) |
| `IceAgeGLSurface_touches{Began,Moved,Ended,Cancelled}(x, y)` | `GUI_Touches*` en píxeles de superficie |
| `IceAgeAndroid_nativeOnBackPressed()` | `EAGLView::OnBackPressed` (pausa en juego / página anterior en menús; `true` = menú principal) |
| `IceAgeAndroid_nativeSetBundlesPurchasedState(p1, p2)` | `pack1/pack2_purchased` (+250 puntos cada uno si score < 500) |
| `utils_{SocialUtils,FacebookWrapper,FyberManager,MoPubManager}_nativeInit(obj)` | guardan objeto/clase receptor |
| `libfmodex: FMODAudioDevice_fmodGetInfo / fmodProcess` | bomba de audio (ver §5) |

## 3. Ciclo de vida reproducido en `source/main.c`

Orden de `IceAgeAndroid.onCreate → postDownloadInit → continueCreating` + hilo GL:

1. `soloader_init_all()` (`source/utils/init.c`): directorios, kubridge, `libfmodex.so` → `libIceAgeAndroid.so`
   (relocate, `resolve_imports` enlaza `FMOD_*` vía `so_resolve_link`), `jni_init()`.
2. `JNI_OnLoad(&jvm)`.
3. `gl_init()` (vitaGL 960x544, MSAA 4x).
4. `nativeSetBundlesPaths(bundle, bundle)` — `bundle` = `DATA_PATH/main.obb` si existe, si no el APK.
5. `nativeInit` de Facebook/Social/Fyber/MoPub con objetos placeholder no-NULL.
6. `createFramebuffer(960, 544)` **antes** de DidFinishLaunching (decisión del port: en Android el
   surface se crea después, y `TexManager_ReloadAllTextures` re-sube todas las texturas; aquí el contexto GL
   ya existe durante `InitGame`, así que invertir el orden evita subir todo dos veces). **Verificar en consola.**
7. `audio_start()` (Java lo arranca en `onWindowFocusChanged`, antes de `continueCreating`).
8. `nativeApplicationDidFinishLaunching("ux0:data/carnivoresiceage", APK_PATH, "ux0:data/carnivoresiceage")`.
9. `nativeResize(960, 544)`, `nativeSetBundlesPurchasedState(true, true)` si `unlock_bundles 1`.
10. Bucle: `input_update()` → `layoutSubviews()` → `gl_swap()`; flush de log cada 5 s.

## 4. Assets [confirmado estático]

- El motor abre el **APK directamente** con su libzip estático: `Files_OpenFileOfType` → `zip_open(apkPath)` →
  `zip_fopen(name, ZIP_FL_NODIR)`; si falla prueba `bundle1Path`/`bundle2Path` (el OBB de Google Play).
  `ZIP_FL_NODIR` ignora directorios, por eso `assets/pack0/*.ogg|.crt|.3dn|...` se encuentran por nombre.
- Los nombres literales muestreados del `.so` (`gui.tga→gui.crt`, `menu_01`, `binoculars`, `compas.3dn`,
  `particle`, `vh/vs.tga`, ...) existen en `assets/pack0/` del APK; `menu_01_ipad` no (solo modo iPad y
  `DidFinishLaunching` fija `is_ipad = 0`) → este APK (44 MB, 188 MB descomprimido) parece traer el contenido del
  OBB (EXPANSION_SIZE 27 MB) integrado. Si faltara algo, copiar el OBB como `main.obb`.
- Texturas `.crt/.crthd`: formato propio; el loader de texturas llama `glCompressedTexImage2D` con
  `GL_COMPRESSED_RGB/RGBA_PVRTC_4BPPV1_IMG` (0x8C00/0x8C02) — vitaGL los soporta.
- Guardado: `basedir/CarnivoresData.dt` → `ux0:data/carnivoresiceage/CarnivoresData.dt`.
- Fotos: `ux0:data/carnivoresiceage/.iceage/photos/%d.png` (glReadPixels + libpng).

## 5. Audio [confirmado estático]

- `Sounds_Init`: `FMOD_System_SetOutput(sys, 0x15)` = `FMOD_OUTPUTTYPE_AUDIOTRACK`. FMOD no toca hardware:
  espera que `org.fmod.FMODAudioDevice` (Java) llame `fmodGetInfo()` y `fmodProcess(ByteBuffer)`.
- `source/reimpl/audio.c` replica `FMODAudioDevice.run()` en un hilo nativo con `sceAudioOut` (puerto BGM,
  granulo múltiplo de 64, FIFO si `dsp_len` no lo es).
- `fmodProcess` obtiene el puntero con `env->GetDirectBufferAddress()` (offset 0x398) → FalsoJNI
  modificado para devolver el propio "objeto" (se pasa memoria nativa).
- `dlopen("libOpenSLES.so")` devuelve NULL para que FMOD no intente OpenSL.

## 6. Java emulado (`source/java.c`) [confirmado estático]

Solo los métodos que el `.so` pide con `GetMethodID` (literales en el pseudo-C). Únicos con retorno usado:
`getCurrentLanguage()` (debe ser string válido: `Locale_Init` hace `strlen`) → idioma del sistema Vita
(en/de/fr/es) o `language` de `config.txt`; `getSnapshot()` → `"{}"`. `isOnline/isSignedIn/isAgsInitialized/requestPurchase` → false.
Campos `internet` y `purchaseManager` devuelven placeholders no-NULL.

## 7. Input (`source/input.c`)

- Táctil frontal → `touches*` en píxeles 960x544 (ID Vita mapeado a slots, nunca se pasa el ID crudo).
- El motor identifica toques **por cercanía** (`GUI_GetTouchByLocation`), sin ID: los movimientos
  sintéticos se parten en pasos ≤12 px lógicos.
- Botones físicos → toques sintéticos sobre los controles reales del HUD, leyendo `gui_controls[]`
  (registros de 0x180 bytes) y los índices exportados (`game_fire`, `game_movement_controller`, ...).
  Solo si el control está activo en el grupo GUI actual.

| Vita | Acción |
|---|---|
| Stick izq. | `game_movement_controller` (joystick virtual, radio 40 px lógicos) |
| Stick der. | cámara directa: hook de `GUI_GetBackgroundMovements()` suma el stick en px lógicos/s (`look_sensitivity`, `invert_look_y`) |
| R / Cruz | `game_fire` |
| L | `game_alternative_fire` |
| Cuadrado | `game_weapon` |
| Triángulo | `game_binoculars` |
| D-pad arriba | `game_call` |
| Select / D-pad abajo | `game_map` |
| Start / Círculo | `nativeOnBackPressed` (pausa / volver) |

## 8. Checklist

- [x] Repo creado desde soloader-boilerplate, git init, .gitignore anti-DMCA.
- [x] APK decompilado (jadx) y .so decompilado(s) (Ghidra).
- [x] Análisis del motor real (ciclo de vida, assets por libzip, FMOD AudioTrack, input por cercanía).
- [x] Bootstrap del loader: 2 módulos, imports 100% resueltos, build OK (`eboot.bin`, `.vpk`).
- [x] Tabla JNI (FalsoJNI): métodos/campos reales del `.so`.
- [x] Gráficos: vitaGL GLES1 fijo, PVRTC.
- [x] Audio: bomba FMOD → SceAudioOut.
- [x] Input: táctil + mapeo de botones/sticks a controles del HUD.
- [x] Logs: `logs/carnivoresiceage_NNN.log` + `next.idx` (`psvita-toolkit log-standard`: OK).
- [x] LiveArea desde recursos del APK (`extras/scripts/make_livearea.py`).
- [ ] **Primer arranque en consola real** ← siguiente paso, requiere hardware.
- [ ] Ajustar sensibilidad de cámara / posiciones de toques sintéticos según lo visto en pantalla.
- [ ] Rendimiento (444 MHz ya fijado; medir FPS con `show_fps 1`).

## 9. Riesgos / hipótesis a verificar en consola

1. Orden `createFramebuffer` antes de `DidFinishLaunching` (§3.6). Si texturas salen negras/corruptas,
   probar el orden de Android.
2. FMOD `AUDIOTRACK` init: si `Sounds_Init` se cuelga, revisar la inicialización del output (sin decompilar
   limpio en Ghidra: `LAB_000c903c`).
3. Páginas de ayuda (`showTutorial`/`setTutorialFile`) son WebViews HTML en Android: en Vita se ve el
   marco pero no el texto.
4. `quick_touch`: toques de fondo < 0.15 s disparan una acción; los arrastres sintéticos duran ≥ 0.25 s.
5. libzip escribe con macros `putc` inline sobre el `FILE*` (solo al *modificar* un zip; el juego solo lee).

## 10. Herramientas

Build/deploy con **psvita-port-toolkit** (standalone): `psvita-toolkit build`,
`psvita-toolkit deploy --vpk` / `--eboot`, `psvita-toolkit analyze <dump>`.

## 11. Estándar de logs en consola

`<DATA_PATH>logs/carnivoresiceage_NNN.log`, 001..999 con `next.idx`. Durante el arranque cada línea se
escribe al instante (un crash no pierde el final); en el bucle principal se bufferiza y se vacía cada 5 s y
en cada error. Incluye `__android_log_print` del juego (`[ALOG]`) y FalsoJNI (`[FalsoJNI]`).
