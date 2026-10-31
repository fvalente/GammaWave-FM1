# Etapa 2 — informe (OpenCode 1 → stage 2)

Base: `main` en `30d0670` (Open Code 1). Rama de trabajo: `cline/13ey37k1`. Nada de lo hecho en el
PR #1, en Devin ni en Open Code se ha revertido ni reescrito.

## Dónde se ha ejecutado esto

Host Linux x86-64, con la toolchain real del objetivo (JieLi pi32v2, `jieli-linux-toolchains-20260730.1`
de `pkgman.jieliapp.com`), el SDK AC79 (3 archivos, los que verifica `tools/build.py`), Python 3.13 con
Pillow / numpy / fonttools, gcc 12, node 24. `./build.py` + `tests/run_tests.sh` completos: no es un
informe sobre lo que no se ha ejecutado, salvo lo que se marca como pendiente abajo.

**No hay ninguna prueba en un FM-1 real en esta etapa.** Nada de lo que sigue dice que una función
suene o funcione en hardware.

## 1. El fallo de CI del parser (`.fwsc`)

Diagnóstico: el fixture escribía una cabecera de flash de 32 bytes *cifrada* donde su entrada UFW
declaraba `dcrc = 0` ("32 zero bytes"). `crc16` de esos bytes cifrados no es 0, así que `parseUfw`
rechazaba el paquete con `flash.bin CRC mismatch` antes de llegar al recorrido de la lista de
entradas que la prueba quería comprobar. La prueba fallaba por el fixture, no por el parser, pero el
parser tampoco se estaba comprobando en ese caso.

Corrección: el test se reorganiza alrededor de un constructor de imágenes real (`buildFlash` /
`buildPackage`) que escribe cabecera de flash, `isd_config.ini` con su chip key (los dos extremos de
`chipkeyDecode`), el área de la app (`app_area_head`, `app.bin`, fin de lista, bloque `cfg`)
cifrada como un solo flujo sfc, y la lista de entradas UFW como la escribe `fm1pkg_make.py`. Las dos
primeras comprobaciones parsean un paquete bueno y leen su layout: si el fixture se rompe, falla ahí
en vez de esconderse detrás de un error plausible.

27 comprobaciones (antes 5), todas alcanzando el camino que nombran: descarga cortada, archivo vacío,
`flash.bin` de tamaño 0, sin terminador de lista, contador de entradas 0xFFFF, `flash.bin` fuera del
paquete, CRC del paquete / de la cabecera / de la lista / del flash, sin `flash.bin`, sin
`isd_config.ini` (sin chip key), sin `app_dir_head`, `app_dir_head` más allá del último bloque
alineado, área de app mayor que la imagen, sin `app.bin`, lista de app sin fin y lista cortada contra
los bytes de la app, `app.bin` fuera de la imagen, área cifrada con otra chip key, sin bloque `cfg`,
y tres cotas de progreso (un paquete de 512 KB con 16 000 ranuras válidas se rechaza en ~20 ms).

Dos endurecimientos del parser, pequeños y verificables:

* un archivo más corto que los 20 bloques de identidad se rechaza como "too short" (antes, todas las
  CRC a cero comparaban iguales y el error era "no flash.bin");
* la lista de entradas de la app se recorre dentro del área de la app (`head.size`), que es
  exactamente lo que el parser descifra: una entrada más allá sigue siendo cifra y no puede validar,
  así que una lista truncada se rechaza al momento en vez de leer cifra como entradas.

Comprobado además contra un paquete **real** construido con la toolchain (`build/felucca.fwsc`,
FM-1_982, 581 564 B de app, `cfg` 1342): parsea igual antes y después de los cambios, y
`tests/run_tests.sh` le pasa la ruta al test cuando existe (el workflow del emulador también).

## 2. DICE ALL

Lo que ya estaba (Open Code) es correcto y ahora está comprobado:

* `gen_power_on()` limpia toda la estructura `gp` y después fija los mismos campos que el inicializador
  estático: equivalente a un arranque real (comprobado campo a campo, incluidos `var`, `svar`,
  `secvar`, `pin`, `arr`, `lock`, `sig`, `last`).
* `ui_power_on()` llama a `gen_power_on()` y el test resetea `host_ticks`: el test no hereda el reloj
  ni los ajustes de un soak anterior.
* `DICE ALL` respeta un TEMPO manual (AUTO = 0 sigue moviéndolo): comprobado en el test.
* El RNG es reproducible donde debe serlo: mismas opciones y misma semilla → misma pieza (nuevo test
  `test_gen_reproducible`), otra semilla → otra pieza. DICE ALL pide semilla nueva en cada tirada
  (lo cubre la comprobación de 40 tiradas: ninguna pieza repetida, tempo siempre distinto).

Nuevo en esta etapa: modo de ejecución en el arnés (`UI_ONLY=dice_all`, `UI_REPEAT_DICE=N`) para
correr el test solo, primero en un proceso nuevo, y N veces seguidas. Resultados reales:

| Ejecución | Resultado |
|---|---|
| `UI_ONLY=dice_all ./ui_test` (solo, proceso nuevo, antes de cualquier otro test) | pasa |
| `UI_REPEAT_DICE=100 ./ui_test` (100 iteraciones, un proceso) | 700 comprobaciones ok, 0 fallos, 2,8 s |
| `UI_REPEAT_DICE=20` con `-DFELUCCA_FM4=1` | pasa |
| suite completa (`ui_test`), donde el test corre tras ~200 otros | pasa |

`tests/run_tests.sh` ejecuta 50 iteraciones después de la suite (3 s): si el orden o el estado dejado
por otro test importara, el CI lo vería.

## 3. Presupuesto DSP (pi32v2)

Medido con la toolchain del objetivo en este árbol (`./build.py` → `build/felucca.dis` →
`tests/target_budget.py`), 38 líneas:

| Línea | Antes | Medido ahora | Estado |
|---|---|---|---|
| `drum_out` | 0 ("any loop" = rojo) | 169 | era **rojo**; ver abajo |
| `drum_side_ramp` | sin línea | 44 | no se comprobaba |
| `DRUM_PATH` | sin línea | 1189 (1191 de presupuesto) | no se comprobaba |
| resto (35 líneas) | sin cambios | dentro de +10 % (perf_block 564/522, fm1_timer5_irq 35 469/33 439, rev_room 239/218, …) | ok |

Hallazgo: `drum_out` tenía presupuesto **0** y el compilador del objetivo mide 169 → la comprobación
estaba en rojo, y nadie lo vio porque el CI no la ejecutaba (necesita la toolchain). El 0 lo escribió
Open Code a mano junto con la entrada en `FUNCS` (no había toolchain: no era una medición). No es un
presupuesto relajado: **nunca pasó**, porque nunca corrió. 169 es la primera medición real, y a partir
de ahora un +10 % falla.

`drum_side_ramp` (44) no tenía línea: cualquier crecimiento pasaba inadvertido. `DRUM_PATH` = **1191**,
la suma de las nueve líneas del camino (así el camino queda exactamente tan ajustado como sus partes);
medido 1189 (`dx_run_sn` 275/276, `drum_mix` 125/126). `.data+.bss` 91 156 B de 98 304 y código en RAM
21 244 B de 24 576 (del propio build, sin cambios en esta etapa).

Reglas nuevas en `tests/target_budget.py`:

* una función de `FUNCS` sin línea de presupuesto **falla** (antes imprimía "no budget" y pasaba): un
  bucle de render nuevo ya no se escapa de la ratchet;
* `DRUM_PATH` sin línea también falla;
* `BUDGET_UPDATE=1` sigue siendo la única vía de escribir una línea, y el fichero y el docstring dicen
  que es para cambios revisados, nunca para poner verde una ejecución.

Qué mide y qué no (escrito en el propio fichero y en el docstring): instrucciones **de los bucles de
una función**, ponderadas por anidamiento y por división hardware, tal como las emitió el compilador
del objetivo. No es un conteo de ciclos, no ejecuta el código, no sigue llamadas (los bucles del
llamado son la línea del llamado), no sabe de memoria, caché ni DMA, y **no es una medición en un
FM-1 ni un margen de CPU**. Coste de una función, coste del camino completo y carga real del
dispositivo son tres cosas distintas.

Sí es posible medirlo en GitHub Actions: el workflow del emulador ya descarga la toolchain en `main`
y construye el paquete, así que ahora ejecuta `tests/target_budget.py` después del build (sin
`continue-on-error`) y `node web/test_pkg_bounds.mjs build/felucca.fwsc` sobre el paquete recién
construido. Los PR no lo ejecutan (la descarga de la toolchain sigue siendo solo en `main`): ver
pendientes.

## 4. Instalador y seguridad

Comprobado (con `tests/install_test.py`, 100 % en verde aquí, y `web/test_web.mjs` / `test_backup.mjs`):

* **SHA-256 obligatorio** en el CLI: sin `--sha256` ni `PACKAGE.sha256` / `SHA256SUMS`, error (exit 2)
  y **ni un byte de MIDI**; cualquier discrepancia, exit 2 y nada escrito. En la web, `versions[i].sha256`
  ausente → mensaje `nohash` y `image` queda `null` (INSTALL deshabilitado); discrepancia o tamaño
  distinto → `badhash`, idem.
* **`--verify-only`**: verifica el paquete (hash, identidad, marca del loader) y el dispositivo
  (modelo), y no escribe nada (el test lo comprueba con `dev.sent == []`); con otro modelo, exit 6.
* **Identidad antes de escribir**: `Updater.install()` compara el modelo del FM-1 con el del paquete
  antes de `check()` / `write()`; el paquete se identifica por `product_of` y la marca
  `FELUCCA-LOADER-1` (o el V15 oficial, o `--force`).
* **Backup antes de flashear** (web, solo firmware GammaWave/JIANT): `captureBackup` se espera
  completa, se descarga y se pide confirmación antes de `up.install()`; si el firmware no deja hacer
  backup, se avisa y se pide confirmación explícita.
* Integridad (SHA-256) y autenticidad (firma) siguen siendo cosas distintas, y así se dice en los
  mensajes y en `docs/PHASE0_SECURITY.md` (propuesta Ed25519: formato, custodia, rotación,
  anti-rollback y qué no resuelve; **sin generar ni publicar claves y sin tocar el camino de
  instalación**).

Cadena de suministro: los tres archivos del SDK se verifican por SHA-256 en `tools/build.py`; la
**toolchain** se descarga sin hash (`tools/get_toolchain.sh`) — pendiente, ver abajo.

## 5. GitHub Actions y emulador

* **El 404 de Pages está diagnosticado**: `GET /repos/Guillel51/Quantum-GammaWave/pages` responde
  **404** → Pages no está habilitado en el repositorio, y `actions/deploy-pages@v4` falla al pedir el
  sitio ("Get Pages site failed"). No es un problema del emulador ni del código: se arregla una vez en
  **Settings → Pages → Build and deployment → Source: GitHub Actions**. No se ha tocado el emulador.
  Se añade un paso previo en el job `deploy` que lo dice con esas palabras en vez de dejar un 404
  (y que no intenta crear el sitio: el token del job no puede).
* Build y smoke test del emulador: ya pasaban y siguen pasando; el artefacto `emulator-<sha>` se
  conserva 14 días en cada push y PR; solo `main` publica.
* `c-cpp.yml` corre en push/PR a `main` y a mano, sin filtros de rutas, con `NO_PACKAGE=1`: tests de
  host, ASan/UBSan y los tres tests web (incluido el del parser). Nada omitido por un filtro.
* `emulator.yml` filtra por `firmware/**`, `web/**`, `tools/**`, `build.sh` y el propio workflow: un
  cambio solo en `tests/**` no lo dispara (el emulador no usa `tests/`), pero un cambio en `web/**` sí.
* No se publican versiones no aprobadas: el instalador público se construye desde `web/STABLE` (un
  commit de `main`), el preview va marcado `-preview`, y `web/fetch_prev.py` solo conserva versiones
  ya publicadas.


## 6. Lo que sigue pendiente (no se da por resuelto)

1. **Backup de firmware que no sea GammaWave/JIANT**: con el V15 oficial encima, el instalador
   instala sin backup (el usuario puede volver a bajar `firmware/FM-1.fwsc`, pero sus ajustes/patrones
   de fábrica se pierden); con otro firmware, se niega a instalar y ofrece el V15. Falta un backup (o
   un aviso equivalente) para el caso "stock".
2. **Restauración interrumpida o parcial**: `restoreBackup` valida todo antes de la primera escritura
   y ordena proyectos, bancos, ajustes y música en vivo al final (`web/test_backup.mjs`), pero un
   corte a mitad de la restauración deja el FM-1 con parte de los objetos escritos; no hay reanudación
   ni transacción.
3. **Checksum de la toolchain**: no se verifica (solo el SDK). Propuesta en `docs/PHASE0_SECURITY.md`
   §6; requiere fijar el hash del tarball que sirve `pkgman.jieliapp.com`, que rota sin aviso.
4. **Carriles punch-in 11/12 con firmware antiguo**: el test de Devin lo cubre cargando en un firmware
   que solo conoce 1..10, pero no se ha probado en un FM-1 con firmware anterior.
5. **Márgenes reales de RAM y RAMTEXT**: 91 156/98 304 B (92,7 %) y 21 244/24 576 B (86,4 %) según el
   linker, sin margen para features nuevas grandes; no es una medición en el dispositivo.
6. **Firma digital**: propuesta solamente (ver §4 y `docs/PHASE0_SECURITY.md`).
7. **Pruebas físicas en el FM-1**: ninguna. Ni los FX nuevos (VIB/DIVE/GATE/SWEEP), ni el instalador,
   ni el backup/restore, ni el emulador se han probado en hardware.
8. **CPU real con carga máxima** (8 voces + batería + FX + reverb): no medida. Lo único que hay es el
   conteo estático del objetivo, el conteo de instrucciones bajo valgrind del host (`cpu_vg.py`) y el
   reloj del emulador; ninguno es el dispositivo.
9. **El presupuesto de target no corre en los PR** (solo en `main`, cuando hay toolchain).
10. **La prueba del V15** (`tests/install_test.py`, restauración oficial) se salta sin
10. **La prueba del V15** (`tests/install_test.py`, restauración oficial) se salta sin
    `FM-1_v15.fwsc`: ese camino sigue sin ejecutarse aquí.
11. **`ui_render` está en rojo y ya lo estaba antes del PR #1** (hallazgo de esta etapa, no arreglado):
    el paso sale con código 1 si hay cualquier hallazgo de lint o cualquier desalineación de 1 px.
    Medido aquí: en el commit anterior al PR #1 (`e06bde2`) 2 hallazgos de lint y 148 medidas
    desalineadas de 2 491 449; en `main` hoy 6 hallazgos y 1 830 de 2 525 677 (y 8 / 1 874 en la
    variante `FELUCCA_FM4=1`). Los 6 hallazgos están todos en MENU > LARGE (valores demasiado anchos
    para su celda: `'355' '1/16' 'ORBIT' '1.18' …`, uno en S: `'SIN LP+'`, y `LARGE WHEEL/PRESETS`
    recortado sin ser texto libre). Es decir: el trabajo de UI del PR #1 (paleta QUANTUM, pantalla
    GEN, capas de performance) multiplicó por ~12 las desalineaciones, pero el paso **nunca** estuvo
    verde, así que el job "host tests" estaba rojo por esto además de por el parser. Necesita una
    pasada de layout (detalles en `build/ui_new/align.txt` y `report.txt`) y, si no se corrige entero,
    una línea base explícita — lo que sería relajar el umbral y por eso no se toca en esta etapa.

    `FM-1_v15.fwsc`: ese camino sigue sin ejecutarse aquí.

## 7. Próxima etapa recomendada

1. Habilitar Pages (§5) y comprobar el despliegue; después, la firma Ed25519 en el orden de
   `docs/PHASE0_SECURITY.md` (primero las claves y el verificador con tests de clave de prueba).
2. Backup/aviso para el firmware de fábrica y una restauración que no deje el dispositivo a medias
   (reanudar por objeto, o escribir solo tras validar el archivo completo en el propio FM-1).
3. Fijar la toolchain por hash y ejecutar el presupuesto de target también en los PR (cache de la
   toolchain ya existe).
4. Medir CPU real en hardware con la carga máxima (contador de ciclos o instrumentación del ISR), y
   con eso decidir si `perf_block`/`fm1_timer5_irq` tienen margen de verdad.
5. Prueba física del FM-1: instalación, backup/restore, FX punch-in y carriles 11/12 con firmware
   antiguo.

Nada de lo anterior se ha empezado: esta etapa se cierra con el CI del parser arreglado y lo demás
comprobado o documentado.

## 8. Resultado real de la suite completa (toolchain y paquete construidos aquí)

`tests/run_tests.sh` termina en `HOST TESTS FAILED`, y los dos únicos pasos que fallan son
`ui_render` (ver §6.11) y su variante `FELUCCA_FM4=1`. Todo lo demás pasa:

| Bloque | Resultado |
|---|---|
| Golden renders / salud / voces / routing | 151 renders, 0 cambiados, 0 fallos de salud, 0 de voces, 14 129 eventos sin nota colgada |
| Presupuesto de target | 38 líneas `ok`, `DRUM path 1189 (budget 1191)` |
| `ui_test` (incluye DICE ALL) | `ui: all passed`; `dice_all x50: all passed` |
| ASan/UBSan (todos los tests) y los dos fuzzers | ok (18 777 frames, 5000 iteraciones) |
| Instalador (CLI), tests web, backup web, parser `.fwsc` | `install tests passed`, `web tests passed`, `backup web tests passed`, `package bounds: all checks passed` (con el paquete real) |
| Emulador WASM | omitido aquí (no hay `emcc`); en CI ya pasaba |

Es decir: el fallo de CI que motivaba esta etapa (el parser) está corregido; el job "host tests"
seguirá en rojo mientras no se aborde `ui_render`, que ya estaba rojo antes del PR #1.
