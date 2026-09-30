# Cómo publicar una release en GitHub

Procedimiento interno (el texto público de la release está en `RELEASE_NOTES.md`).

## 1. Verificación en consola (antes de etiquetar)

- [ ] Instalar el VPK recién compilado y arrancar con `CarnivoresBundleOne.apk` / `CarnivoresBundleTwo.apk`
      en `ux0:data/carnivoresiceage/`.
- [ ] En el log más nuevo (`ux0:data/carnivoresiceage/logs/`):
  - [x] `zip: ...CarnivoresBundleOne.apk opened (kept open)` y lo mismo para `BundleTwo` (confirmado, log 007).
  - [ ] No aparece `zip: unexpected opcode` ni `inlined ferror() sites patched`.
  - [ ] Eligiendo la escopeta doble ya **no** aparece `weapon 'dbsgun' has no model/animations`.
- [ ] Área 3/4 (pack 1) y área 6 (pack 2) cargan; rifle de francotirador y ballesta se ven con su modelo.
- [ ] Sin los packs: el juego sigue arrancando (fallback a `rifle`, sin congelarse con Cuadrado).
- [ ] En la cacería los botones táctiles casi no se ven (1 %) y siguen respondiendo al toque; la brújula se ve
      normal; Círculo hace la llamada y Start pausa.
- [ ] Salir con PS y volver a entrar: la partida guardada se conserva.

Si los packs siguen sin abrir: publicar igual, pero mover "Optional content packs" de *What works* a
*Known issues* en `RELEASE_NOTES.md`.

## 2. Build

```sh
export VITASDK=~/vitasdk PATH=$VITASDK/bin:$PATH
rm -rf build && mkdir build && cd build && cmake .. && make
```

Para una versión nueva, subir `VITA_VERSION` en `CMakeLists.txt` (`01.00` → `01.01`, ...) y el título de
`RELEASE_NOTES.md`.

## 3. Revisar que no se filtra nada del juego

```sh
git status --ignored   # APK, .so, carnivoresiceage_extract/, decompiled/, ux0_data/, logs/ deben estar ignorados
git ls-files | grep -iE '\.(apk|obb|so|zip|crt|crthd|ogg|3dn|ani|car)$'   # tiene que salir vacío
unzip -l build/carnivoresiceage.vpk     # solo eboot.bin, sce_sys/ (param.sfo + LiveArea)
```

Las imágenes de `extras/livearea/` se generan desde el arte del APK (`extras/scripts/make_livearea.py`):
decidir si se publican o se reemplazan por arte propio.

## 4. Commit, tag y push

```sh
git push origin master        # o merge a main si ése es el branch por defecto en GitHub
git tag -a v1.0 -m "Carnivores: Ice Age for PS Vita v1.0"
git push origin v1.0
```

## 5. Crear la release

```sh
gh release create v1.0 build/carnivoresiceage.vpk \
    --title "Carnivores: Ice Age for PS Vita v1.0" \
    --notes-file RELEASE_NOTES.md
```

`RELEASE_NOTES.md` enlaza al `README.md` con ruta relativa: en la página de la release conviene cambiarlo por
el link absoluto del repo (`https://github.com/<usuario>/<repo>#installation`).

## 6. Después

- Anotar en `port_progress.md` la versión publicada y el resultado de la verificación.
- Issues: pedir siempre el `.log` más nuevo y, si hay crash, el `.psp2dmp` (ver `so-crash-triage`).
