# Auditoría del PR #1 (etapa 1) — Quantum GammaWave v1.4

Base: `devin/1793394967-quantum-v1.4` (053763d). Rama de trabajo: `cline/hrqxkss8`.
54 archivos, +1161/-216 respecto de `main`.

## Ejecutado en este entorno (host, sin toolchain JieLi, sin emcc, sin Pillow)
| Prueba | Resultado |
|---|---|
| `tests/perform_test.c` (VIB/DIVE/GATE/SWEEP, carril 11/12, clicks, bit a bit) | PASA. El coste ("cost") NO se mide: no hay contador de instrucciones |
| `tests/install_test.py` | PASA. Se omiten la comparación con fm1pkg.js y la restauración V15 (no hay felucca.fwsc ni FM-1_v15.fwsc) |
| `web/test_web.mjs` | Los tests del editor pasan; `packages` omitido (requiere ./build.sh) |
| `ui_test` (DICE ALL), regress/golden, presupuesto de target, emulador WASM | NO ejecutados: faltan Pillow/fontTools (fuentes UI), toolchain pi32v2 y emcc |

No se afirma nada sobre lo no ejecutado. Las cifras de CPU/RAM del PR (perform_block 564 vs 522, RAM 91156/98304) son de Devin y no se verificaron.

## Funcionalidad nueva
- Punch-in FX (`perform.c`, `pfx.c`, `seq.c`, `ui_layer.c`): VIBRATO G#4 y DIVE F#5 (de nota, vía `pfx_pit`, códigos de carril 11/12), GATE C#5 y SWEEP D#5 (master, en `perf_begin/perf_block`). `PF_IS(e)` reemplaza `e < PF_N`.
- Emulador: workflow CI + `tools/emu_dev.py`.
- Instalador: SHA-256 (web+CLI), CSP, checklist, verify-only, backup, lanzadores, `docs/INSTALL.es.md`.
- Presets/PHASER/paleta QUANTUM vienen de commits previos (v1.3).

## Hallazgos
1. **SHA-256 sin autenticidad**: el hash viaja en el mismo sitio que el paquete (`versions.json`); detecta corrupción, no manipulación. No debe presentarse como firma. Falta firma o hash publicado por un canal independiente.
2. **Hash opcional en web**: `index_pkg.html:648` solo valida `if (v.sha256 && ...)`; un paquete sin hash se instala (solo muestra el hash). Debería rechazar.
3. **Backup solo para JIANT/GammaWave** (`fw.kind === "jiant"`, línea ~732). Otros firmwares: no hay backup previo. Revisar que lo avise claramente.
4. **CLI**: `--verify-only` solo comprueba modelo; el chequeo de versión/compatibilidad de firmware no existe. No se declara compatibilidad con versiones no probadas. `load_package.digest` es estado en atributo de función (frágil). Un `.sha256` ausente solo omite la verificación sin avisar.
5. **DICE ALL**: el test falla solo en la suite completa. `ui_test` usa estado global compartido (`rng_state`, `host_ticks` avanzado con `SOAK_RND`, ejemplo línea 2602) que depende del orden de los tests previos; aislado pasa con otra secuencia de semillas. El umbral se relajó en 053763d (>=2 motores por tirada, media 5/6): es una tolerancia estadística, no una corrección. Pendiente: reproducir con la suite completa y sembrar `rng_state` de forma determinista al inicio de `test_dice_all`, o medir la tasa de fallos sobre muchas semillas. No pude reproducirlo aquí (no se compila ui_test sin las fuentes generadas).
6. **drum_render**: el commit 053763d movió la etapa de salida a `drum_out/drum_add` (155→10); el coste total no se reduce, se redistribuye a otras entradas del presupuesto (`drum_add 22`). El presupuesto por función oculta el coste real; conviene sumarlo. Sin pi32v2 no se verificó que el audio sea idéntico bit a bit (afirmación de Devin, respaldada por goldens).
7. **Margen de RAM**: .data+.bss 91156/98304 B (92.7 %), código RAM 21244/24576 B (86.4 %); poco margen para nuevas funciones. Polifonía en NVOICE 8.
8. **perform_block +8 %** del presupuesto aceptado por debajo del umbral 10 %: cada nuevo FX lo consume.
9. **Compatibilidad**: carriles con códigos 11/12 cargan en firmware viejo (solo 1..10); verificado en el test de Devin, no en hardware.
10. **Sin pruebas en hardware** de nuevos FX ni del instalador (admitido en el PR).
11. Docs: MANUAL.md/es.md muestran diffs de 3000+ líneas en commits intermedios (cambio de finales de línea), el diff final contra main es pequeño; vigilar CRLF.

## Propuestas para la etapa 2
1. Reproducir DICE ALL con la suite completa (instalar Pillow/fontTools, o generar ui_fonts) y fijar semilla determinista.
2. Hacer obligatorio el hash en la web y avisar en el CLI si falta.
3. Backup/aviso para todo firmware origen, y tabla de compatibilidad solo con versiones verificadas.
4. Medir coste en target (toolchain pi32v2) y reportar la suma de drum_*.
5. Prueba en hardware de VIB/DIVE/GATE/SWEEP.
