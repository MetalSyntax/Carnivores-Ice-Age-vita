# Plan General — Características Extras para los Ports (gap vs. escena)

> Objetivo: llevar los ports propios (Zenonia 2/3/4, Dungeon Hunter 2/3, Asphalt 5/6,
> Advena, ILLUSIA 1/2, Inotia 3, resto Gameloft/Gamevil) al nivel de extras de los
> ports de referencia de internet (TheFlow, Rinnegatamante, gl33ntwine/v-atamanenko,
> frobnicator, Nevak, etc.), con un diseño **común y reutilizable** en vez de
> parches ad-hoc por juego.
>
> Verificación local (2026-09-22): ninguno de los 6 repos muestreados
> (`Zenonia2/3/4-vita`, `Dungeon-Hunter-2-vita`, `Asphalt-5-Vita`, `Advena-Vita`)
> tiene `trophy/`, configurator app, `settings.bin`, `manual/`, ni `controls.txt`.
> Ese es el gap real, no una suposición.

## Fuentes analizadas

| Port / repo | Extras que aporta (y los nuestros no tienen) |
|---|---|
| `TheOfficialFloW/gtasa_vita` (GTA SA) | Configurator app desde LiveArea, `controls.txt` remapeable, L2/R2→rear touch y L3/R3→front touch (conmutable), teclado OSK con L+SELECT para cheats, quick-save al salir + Resume=carga último save, canciones de radio recortadas restaurables (`MUSIC.md`), MP3 fuzzy seek, texturas HD opcionales, render PS2-like conmutable |
| `Rinnegatamante/raider-vita`, `ff4_vita`, `fahrenheit-vita`, `not_a_hero-vita` | **Trofeos** (incl. ocultos + iconos hi-res), multilenguaje (hasta 7 idiomas, TR2), FMV por transcode ffmpeg `.bat` opcional, `datafiles.zip` separado, front-touch mapping L2/L3/R2/R3, nota PSVshell 500 MHz |
| `v-atamanenko/soloader-boilerplate` (Solobop) | `settings.bin` persistente (`source/utils/settings.c`), redirect `-config` a binario configurador, overclock 444/222/222/166 al arrancar, `SHADER_FORMAT` GLSL/CG/GXP + `DUMP_COMPILED_SHADERS`, targets CMake `send/dump/reboot` vía `PSVITAIP` |
| `v-atamanenko/masseffect-vita`, Backstab HD, Dead Space (gl33ntwine) | Companion/configurator lanzable con botón **Settings en el LiveArea**, deadzones de sticks, nivel de detalle gráfico, FPS limiter, sensibilidad, `Control Scheme 2` documentado |
| `Nevak/bgda-vita` (Dark Alliance) | L2/R2 en rear touch, idioma = idioma del sistema Vita, coop 2 mandos PSTV, Battery Saving = lock 30 FPS, logros marcados como "not yet" (roadmap honesto) |
| Escena general (VitaDB Downloader, reRescaler, VitaGrafix, PSVshell, reVita) | Overclock/Manual FPS counter como expectativa del jugador, VitaGrafix-style res/FPS, manual LiveArea, bg music en LiveArea/Downloader, auto-updater + changelogs visibles |

### Gap analysis resumido

| # | Extra de la escena | Estado en nuestros ports | Prioridad |
|---|---|---|---|
| 1 | Remapeo de controles (`controls.txt` + esquema Vita mejorado) | ❌ Hardcodeado en `source/input.c` | P0 |
| 2 | Navegación de menús con botones físicos además de pantalla táctil | ❌ Solo táctil en menús | P0 |
| 3 | L2/R2/R3 trasero/delantero + soporte PSTV real | ❌ Parcial / sin documentar | P0 |
| 4 | Deadzones + sensibilidad configurables | ❌ | P0 |
| 5 | Configurator app + botón Settings en LiveArea + `settings.bin` | ❌ (boilerplate lo soporta, ningún port lo usa) | P0 |
| 6 | Overclock automático 444 MHz + mención PSVshell | ⚠️ Parcial (algunos lo fijan, sin opción ni docs) | P0 |
| 7 | Limpieza de UI / Desactivar Facebook y servicios online huérfanos | ❌ Botones zombi visibles en HUD/menús | P0 |
| 8 | Trofeos integrados en el SO (`sce_sys/trophy/`, `NPWR...`, `TROPHY.TRP`) | ❌ | P1 |
| 9 | Idioma = idioma del sistema + fallback | ❌ (rutas `eng/` hardcodeadas) | P1 |
| 10 | LiveArea premium (manual, frames, bgm, botón config) | ⚠️ Solo bg/pic/icon básicos | P1 |
| 11 | Quick-save/auto-resume + import saves Android | ❌ | P1 |
| 12 | Cheat/debug OSK (L+SELECT) donde aplique | ❌ | P1 |
| 13 | Opciones gráficas (res interna, MSAA/bilinear, FPS limiter, PostFX/CRT) | ⚠️ Solo flags de build sueltos | P1 |
| 14 | Shader cache en disco + primer arranque rápido | ⚠️ Inconsistente | P1 |
| 15 | Videos/FMV con script transcode + skip elegante si falta | ⚠️ Solo Asphalt 5 lo hace bien | P2 |
| 16 | `datafiles.zip` + checker de archivos + mensajes de error con diálogo | ⚠️ Desigual | P2 |
| 17 | Parches version-agnostic + multi-versión APK soportada | ❌ (un APK exacto por port) | P2 |
| 18 | Release estándar VitaDB (screenshots, trailer, changelog, QR, gamefiles.zip) | ⚠️ Desigual | P2 |

## Fase P0 — Quick wins (1–2 tardes por port, alto valor jugador)

### P0.1 Controles remapeables de forma personalizada (`controls.txt`)
- **Diseño del formato:** Implementar lectura de `ux0:data/<juego>/controls.txt` con sintaxis clave-valor directa `BOTON_VITA = ACCION_JUEGO`.
  - Botones físicos mapeables: `CROSS`, `CIRCLE`, `SQUARE`, `TRIANGLE`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `SELECT`, `START`, `DPAD_UP`, `DPAD_DOWN`, `DPAD_LEFT`, `DPAD_RIGHT`.
  - Mapeo de acciones abstractas (específico por motor o HUD sintético):
    - Ej. en juegos con HUD táctil (Carnivores Ice Age/Dino Hunter): `FIRE`, `ALT_FIRE`, `WEAPON_CYCLE`, `BINOCULARS`, `CALL`, `MAP`, `PAUSE_BACK`, `CAM_RESET`.
    - Ej. en RPGs/Hack'n'Slash (Zenonia/Dungeon Hunter): `ATTACK`, `SKILL_1`..`SKILL_4`, `DODGE`, `POTION_HP`, `POTION_SP`, `INVENTORY`, `MAP`.
- **Zonas táctiles personalizables:**
  - Panel táctil trasero (Rear Touch): cuadrantes independientes superior/inferior para L2/R2 y L3/R3.
  - Pantalla táctil delantera (Front Touch): zonas de esquina configurables para evitar toques accidentales.
- **Deadzones y sensibilidad analógica:**
  - Parámetros en archivo o settings: `DEADZONE_INNER` (por defecto 8-12% para cortar stick drift), `DEADZONE_OUTER` (para alcanzar 100% de vector en diagonales), `LOOK_SENSITIVITY_X`, `LOOK_SENSITIVITY_Y`, `INVERT_LOOK_Y`.
- **Fallback transparente:** Si `controls.txt` no existe, usar el esquema por defecto óptimo para PS Vita (cero regresión).
- **Documentación:** Generar tabla de mapeo por defecto en el README.md del port.

### P0.2 L2/R2/R3 touch + PSTV
- Estándar escena: **L2/R2 = rear touch arriba, L3/R3 = front touch abajo**,
  con alternativa L2/R2 = front arriba (la que añadió GTA SA v1.2 para PSTV).
- Donde el juego no necesite L2/R2, mapearlos a acciones útiles
  (poción/mapa/save rápido) en vez de dejarlos muertos.
- Probar explícitamente en PSTV (o declarar "PSTV requiere DS3/DS4 por MiniVitaTV").

### P0.3 Deadzone + sensibilidad
- Exponer 2 valores en settings (default: inner ~8–12): evita drift y quejas de
  "stick no llega al máximo en diagonal" (cf. AnalogsEnhancerKai).
- Backstab HD lo pide hasta en su guía ("bajar sensibilidad al mínimo") — mejor
  poner defaults sanos desde el loader que pedirlo al jugador.

### P0.4 Configurator mínimo + `settings.bin`
- Activar lo que Solobop ya da gratis: `settings.c` + redirect `-config` +
  botón **Settings en LiveArea** (ejemplo: Mass Effect, Backstab HD).
- Contenido mínimo v1: deadzones, detalle gráfico (si el port tiene flags),
  FPS limiter on/off, toggle overlay táctil (Zenonia 4 ya oculta gamepad virtual;
  hacerlo setting en vez de `#define`).
- El toolkit (`psvita-toolkit`) puede generar el esqueleto del configurador al
  crear/adoptar un port — no copiarlo a mano en cada repo.

### P0.5 Relojes + PSVshell
- Fijar `444/222/222/166` al arrancar (ya lo hace `soloader_init_all()`; verificar
  que todos los ports lo llamen) + línea estándar en README:
  "Opcional: PSVshell a 500 MHz si algún nivel cae".

### P0.6 Limpieza de UI / Desactivación de Facebook y servicios de red huérfanos
- **Justificación y contexto:** Los ports de Android a PS Vita se ejecutan en un entorno estrictamente offline (capacidades de red/HTTP/APIs sociales despojadas o sustituidas por stubs de FalsoJNI). Dejar botones sociales o de inicio de sesión online en pantalla ("Login to FB", "Share hunt to Facebook", "Share trophy to Facebook", "GGS Sign-In") genera una mala experiencia de usuario:
  - Ensucia la interfaz con botones que no cumplen ninguna función.
  - Confunde a los jugadores y testers ("¿el juego se congeló?", "¿por qué falló la conexión?").
  - Si el motor nativo espera un callback de red o bloquea el hilo esperando respuesta tras pulsarlos, puede ocasionar bloqueos de la UI o demoras innecesarias.
- **Estrategia técnica de eliminación:**
  1. **Ocultamiento por GUI:** Localizar en el `.so` o pseudo-C de Ghidra las referencias a los controles (`GUI_AddControl(&ctrl_facebook, ...)` o `GUI_SetControlVisible/Active`).
  2. **Parcheo estático o en runtime:**
     - En runtime (`source/patch.c`): Interceptar o forzar `GUI_SetControlVisible(ctrl, false)` y `GUI_SetControlActive(ctrl, false)` en el bucle principal.
     - En el menú de opciones: Omitir la creación del botón `menu_options_facebook_login` o redirigir sus coordenadas fuera de pantalla.
     - En pantallas de resultados y salas de trofeos: Desactivar `game_share_statistics_with_facebook`, `game_share_hunt_statistic_with_facebook`, `game_share_trophy_with_facebook`, `game_share_trophy_statistic_with_facebook`.
  3. **Stubs limpios:** En `source/java.c`, asegurar que `Facebook_Login`, `Facebook_Logout`, `publishFeed`, etc., retornen inmediatamente sin realizar peticiones ni almacenar buffers zombi.

### P0.7 Navegación de menús con botones físicos además de pantalla táctil
- **Problema en ports puramente táctiles:** En juegos diseñados originalmente para teléfonos con pantallas táctiles capacitivas, toda la navegación fuera de la partida (menú principal, selección de cazador/armas/mapas, opciones, inventario, pausa, confirmación de cuadros de diálogo) exige que el jugador suelte los controles físicos para pulsar la pantalla con los dedos. En una consola portátil dedicada como PS Vita, esto rompe la ergonomía y la inmersión.
- **Mecanismos de navegación híbrida (Botones + Touch):**
  1. **Navegación por foco direccional (D-Pad / Stick Izquierdo):**
     - En pantallas de menú (cuando el HUD de gameplay no está activo): el D-Pad o stick izquierdo navega secuencialmente o por proximidad direccional geométrica entre los botones y controles activos (`gui_controls[]` con `usable = true`).
     - Dibujado de retícula o resaltado sutil en el botón enfocado para retroalimentación visual clara.
  2. **Acciones de confirmación y selección (`CROSS`):**
     - Al pulsar `Cruz` (o el botón configurado en `controls.txt`), el sistema inyecta un tap sintético (`touchesBegan` seguido de `touchesEnded`) exactamente en las coordenadas centrales (`control_center_surface`) del control enfocado.
  3. **Acciones de retroceso y cancelación (`CIRCLE` / `START`):**
     - El botón `Círculo` invoca el retroceso nativo (`nativeOnBackPressed` o tecla Android Back), permitiendo volver a la pantalla anterior, cerrar ventanas modales o retroceder en menús sin necesidad de buscar el botón "Back" táctil.
  4. **Cursor Virtual suave (alternativa para sliders/mapas complejos):**
     - Para interfaces con barras de desplazamiento, selectores de dificultad analógicos o mapas detallados, permitir un puntero virtual manejado con el stick analógico izquierdo, donde `Cruz` simula un toque sostenido o arrastre.
  5. **Convivencia 100% transparente:**
     - La pantalla táctil delantera nunca se deshabilita; el jugador puede alternar instantáneamente entre tocar la pantalla o pulsar los botones físicos en cualquier momento sin necesidad de cambiar modos ni configurar nada.

## Fase P1 — Nivel escena (1 semana por port aprox.)

### P1.1 Trofeos integrados con PS Vita (Nativos)
- **Arquitectura de trofeos en PS Vita:**
  - Seguir el estándar de referencia de la escena (`raider-vita`, `ff4_vita`): paquete `sce_sys/trophy/NPWRXXXXX_00/TROPHY.TRP` generado con las herramientas de empaquetado de trofeos de Sony / VitaSDK (`trp_packer` o similar), con iconos de 50x50 para cada trofeo, un icono general del set y nombres/descripciones localizados.
  - Implementar un submódulo común y reutilizable (`source/utils/trophy.c` / `trophy.h`) basado en la API oficial del sistema `SceNpTrophy`:
    - Inicialización al arrancar: `sceNpTrophyInit(&trophy_opt)` y creación de contexto con el communication ID del juego (`NPWR...`).
    - Desbloqueo asíncrono / no-bloqueante: Cola de trofeos procesada en un hilo ligero para que `sceNpTrophyUnlockTrophy()` nunca cause caídas de frames ni tirones en el juego.
    - Finalización limpia en `exit`: `sceNpTrophyDestroyContext()`.
- **Integración con el motor del juego y puente JNI:**
  - En motores basados en Java/JNI (como Carnivores Ice Age o Zenonia):
    - Conectar el callback de logros JNI (ej. `unlockAchievement(int id)` en `source/java.c`, recién reparado) para llamar directamente a `trophy_unlock(id + 1)` (ajustando índices 0-based a 1-based de la TRP).
    - Los eventos de progreso o estadísticas (`IngameTrophyInfo`, dinosaurios cazados, supervivencia, puntuación récord) se traducen en trofeos del sistema.
  - En motores C/C++ nativos: hookear la función interna de recompensas/logros.
- **Experiencia de usuario idéntica a juegos oficiales de Vita:**
  - Notificación emergente nativa (pop-up oficial de la consola en la esquina superior izquierda).
  - Reproducción del sonido clásico oficial de trofeo de PS Vita.
  - Registro e historial persistente en la aplicación oficial **Trofeos** del LiveArea de la consola.
  - Plena compatibilidad con plugins como `TrophyShot` (captura automática al ganar trofeo) y `TropHAXSE`.
  - Definir una lista balanceada (Platino si la cantidad lo justifica, Oro para hitos mayores, Plata y Bronce), incluyendo trofeos ocultos para evitar spoilers de historia.

### P1.2 Idioma del sistema
- Leer idioma de Vita (`sceAppUtilSystemParamGetById`) y mapear a carpeta de
  assets (`eng/spa/fra/...`); fallback a inglés. Como `bgda-vita`.
- Aplica directo a Zenonia/DH/Inotia (ya traen varias carpetas de idioma en el APK).

### P1.3 LiveArea completo
- `template.xml` con frames + botón Settings/Config + **manual** (`sce_sys/manual/`,
  convertir FAQ actual con FAQ-to-Manual) + bgm `at9` opcional.
- El módulo `livearea.py` del toolkit ya valida specs; añadirle plantillas
  "premium" (manual + botón config) en vez de hacerlo a mano por port.

### P1.4 Saves: quick-save + import Android
- Quick-save al elegir Quit (patrón GTA SA) + Resume = último save.
- Script/doc "copia tu save de Android a `ux0:data/<juego>/`" (ruta exacta por juego).

### P1.5 Opciones gráficas en runtime (no solo flags de build)
- Mover NEON/turbo/downsample/frameskip de `build.sh --flags` a settings:
  `Detalle: Alto/Bajo`, `FPS lock: 30/60/off`, `Filtro: bilinear/sharp`,
  `Velocidad: 1x–3x` (el ciclo `R+SELECT` de Zenonia 4 es buen patrón, pero que
  sea setting persistente, no solo toggle volátil).
- CRT/PostFX solo si vitaGL lo da gratis; no inventar shaders propios por port.

### P1.6 Shader cache
- `DUMP_COMPILED_SHADERS=ON` + avisar "primer arranque tarda, los siguientes no"
  (Solobop ya lo implementa; activarlo y documentarlo en todos).

## Fase P2 — Pulido release (cuando P0+P1 estén)

- **FMV**: patrón `raider-vita`/Asphalt 5: `.bat`+ffmpeg opcional, skip elegante si
  falta el `.mp4` (nunca colgar).
- **Checker de datos**: al arrancar sin `.so`/assets, diálogo `SceCommonDialog`
  con la ruta exacta que falta (no crash mudo `C2-12828-1`).
- **`datafiles.zip`** separado del VPK para lo convertible (texturas/shaders CG).
- **Parches version-agnostic** (patrón `ff4_vita`): buscar por firma, no por offset
  duro, y listar APKs probadas en el README.
- **Release checklist VitaDB**: VPK + screenshots + trailer 30 s + changelog +
  `gamefiles.txt` + QR Brewology + nota kubridge/FdFix/libshacccg.

## Orden de aplicación sugerido (tus repos)

1. **Carnivores-Ice-Age-vita** (candidato inmediato: bug de logros resuelto; aplicar `controls.txt` personalizado para HUD, limpieza de botones de Facebook y habilitar `TROPHY.TRP` conectado a `unlockAchievement`).
2. **Asphalt-5-Vita** (el más maduro: ya tiene controles documentados, audio fixed-point,
   video-skip) → piloto de P0.1+P0.4+P1.5.
3. **Zenonia4-vita** (turbo/frameskip ya existe) → convertir flags a settings P0.4+P1.5.
4. **Dungeon-Hunter-2-vita** → P0.2+P0.5 (ayuda al FPS bajo sin tocar el motor).
5. **Zenonia2/3, ILLUSIA, Inotia3, Advena** → aplicar el paquete P0 ya rodado en masa.
6. Trofeos nativos (P1.1) en oleada común una vez el configurador exista (mismo `settings.bin`
   puede guardar progreso de trofeos si hace falta).

## Automatización con el toolkit (qué hace la máquina, qué pide IA)

> Estado real auditado 2026-09-22 con `psvita-toolkit log-standard`:
> `asphalt5` conforme (0 discrepancias, es el estándar canónico);
> `advena` 3 (sin `next.idx`, path hardcodeado, sin flag de producción);
> `dungeon-hunter-2` 4 (basename `log_NNN` sin slug, base 000, sin `next.idx`, path hardcodeado);
> `zenonia2` sin logger (esquema viejo `log_<timestamp>.txt`).

| Extra del plan | Toolkit lo automatiza | Requiere IA / mano |
|---|---|---|
| Subir `logs/`+`saves/` vacíos por FTP | ✅ Sí — `upload_ux0_data_dir` ahora crea dirs vacíos (fix); `scaffold_ux0_data` deja `.gitkeep` (el uploader lo salta como payload pero crea el dir remoto) | — |
| Estándar `<slug>_001..999` + `next.idx` + `DATA_PATH` + flag prod | ✅ `log-standard [--fix-dirs]` audita y crea staging; el `PORTING_PLAN.md` nuevo ya trae la sección 6 con la norma | Migrar cada logger a mano (plantilla: el de Asphalt 5), la herramienta solo señala el gap |
| Trazas `[TRACE] >> f()` detalladas sin IA | ✅ `log-trace [--dry-run] [--ensure-flag] [--remove]` — regex determinista, entry-only (las de salida con returns/gotos mentirían), idempotente, bloque `PORT_TRACE` (ON=debug, OFF=producción, verificado que compila con `gcc -fsyntax-only`) | La IA las *lee*, no las escribe: con trazas homogéneas el `export-context` le da a cualquier copiloto el mismo formato en todos los ports |
| Controles remapeables, configurador, trofeos, LiveArea premium | ⚠️ Parcial — `sync-shared` propaga un componente ya hecho (ej. `controls.txt`, `settings.c`) a toda la familia de motor; `livearea --validate` y `jni-analyze` cubren su parte | Diseñar el componente una vez (en el piloto Asphalt 5) sí es trabajo de port |
| Release VitaDB | ⚠️ `build`+`deploy`+`analyze` ya headless para CI | Screenshots/trailer/changelog a mano |

Flujo recomendado por port: `log-standard --fix-dirs` → migrar logger → `log-trace --ensure-flag`
para cazar el bug → `log-trace --remove` (o build con `PORT_TRACE=OFF`) antes del VPK final →
`export-context` para que la IA diagnostique sobre logs homogéneos.

## Criterio de "hecho" por port

- [ ] `controls.txt` funciona, permite remapeo personalizado de botones/touch y está documentado
- [ ] Navegación de menús con botones físicos (D-Pad/Cruz/Círculo) funcionando en paralelo a la pantalla táctil
- [ ] L2/R2/R3 tienen función en Vita y PSTV
- [ ] Botón Settings en LiveArea abre configurador (deadzones, detalle, FPS lock)
- [ ] Relojes fijados + nota PSVshell en README
- [ ] Elementos sociales/online huérfanos (Facebook/GGS) desactivados u ocultados limpiamente
- [ ] Trofeos nativos de PS Vita integrados (`TROPHY.TRP`, pop-up oficial, registro en LiveArea y testeado con TrophyShot/TropHAXSE)
- [ ] Idioma sigue al sistema con fallback
- [ ] LiveArea con manual + frames, sin warning `0x8010113D`
- [ ] Release con changelog, screenshots, `gamefiles.txt`, versiones APK probadas

## Notas anti-fabricación

- No prometer online/multijugador resucitado, 60 FPS fijos donde el motor no da,
  ni contador GPU (PowerVR no lo expone a homebrew) — la telemetría honesta es
  frame time, como ya hace `perf_telemetry.py`.
- Todo lo de arriba existe y está referenciado en ports públicos; nada requiere
  investigación nueva, solo portar el patrón al esqueleto común y rodarlo por
  todos los juegos.
