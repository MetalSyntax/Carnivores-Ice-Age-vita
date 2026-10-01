# Cómo publicar una release en GitHub

Procedimiento interno (el texto público de la release está en `RELEASE_NOTES.md`).

## 1. Verificación en consola (antes de etiquetar)

- [ ] Instalar el VPK recién compilado con `main.33.com.tatemgames.iceage.obb` en `ux0:data/carnivoresiceage/`
      (subido por FTP el 2026-10-01).
- [ ] En el log más nuevo (`ux0:data/carnivoresiceage/logs/`):
  - [ ] `bundle 1: ...main.33.com.tatemgames.iceage.obb, bundle 2: ...` (el mismo OBB para los dos packs).
  - [ ] `Marking both content bundles as owned (unlock_bundles=1).`
  - [ ] `zip: ...main.33.com.tatemgames.iceage.obb opened (kept open)`.
  - [ ] Eligiendo la escopeta ya **no** aparece `weapon 'shotgun' has no model/animations`.
- [ ] Zonas 2-3 (pack 1) y 4-5 (pack 2) cargan con cielo y terreno normales (sin morado); escopeta, rifle de
      francotirador, escopeta doble y ballesta se ven con su modelo.
- [ ] Sin el OBB: `Content packs not unlocked` en el log, solo la zona 1 elegible, el juego arranca y la
      escopeta usa el modelo de otra arma (sin congelarse con Cuadrado).
- [ ] Menús con botones: el marcador blanco aparece en menú principal, opciones (sliders con izquierda/derecha),
      páginas de caza, pausa y estadísticas; X selecciona, Círculo vuelve. No hay botones de Facebook.
- [ ] En la cacería: sacar el arma (Cuadrado) y disparar (R/X) funciona; los botones táctiles casi no se ven
      (1 %) y siguen respondiendo al toque; la brújula se ve normal; Círculo hace la llamada y Start pausa.
- [ ] Salir con PS y volver a entrar: la partida guardada se conserva.

## 2. Build

Con el toolkit: `psvita-toolkit build` (compila en un directorio temporal copiado con
`rsync --filter ':- .gitignore'`, así que todo lo que el build necesita tiene que no estar ignorado -- ojo: en
ese filtro `!` no niega, vacía las reglas). A mano:

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
