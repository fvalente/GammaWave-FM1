# Instalar Quantum GammaWave en el FM-1 (y recuperarlo)

## Antes de empezar (obligatorio)
1. Cable USB **de datos**, directo a la computadora (sin hub).
2. Computadora enchufada y sin suspensión; FM-1 con carga.
3. Cerrá todo lo que use MIDI (DAW, Web Studio, otras pestañas).
4. Si tu FM-1 tiene **otro firmware custom** (ni V15 oficial ni GammaWave/JIANT): volvé primero a la **V15 oficial**.

## Opción A — Instalador web (Chrome / Edge)
- Abrí `webapp/installer/` del sitio. El paquete se verifica con su **SHA-256** publicado; si no coincide no se escribe nada.
- **Verificar solamente**: identifica el FM-1 y dice si el paquete se puede instalar, sin escribir.
- Marcá las 4 casillas de la lista previa: recién ahí se habilita **INSTALL**.
- Sobre GammaWave/JIANT, el instalador **descarga primero un backup** (canciones, sonidos, ajustes) y te pide confirmación.
- La página corre con una Content-Security-Policy estricta: sólo su propio script, nada de otros sitios.

## Opción B — Un clic (sin navegador)
- Linux/macOS: `tools/install/install-fm1.sh [paquete.fwsc]` (macOS: doble clic en `install-fm1.command`).
- Windows: doble clic en `tools\install\install-fm1.bat`.
- Crean un entorno de Python propio (`~/.fm1-installer`) con `mido` + `python-rtmidi`, hacen **--verify-only** y piden confirmación.
- Manual: `python3 tools/fm1_install.py paquete.fwsc [--sha256 HEX] [--verify-only]`. Si hay un `paquete.fwsc.sha256` o
  `SHA256SUMS` al lado, se verifica solo.

## Si algo sale mal
| Qué pasa | Qué hacer |
| --- | --- |
| Se cortó durante la escritura | Reconectá y repetí la instalación: el FM-1 queda en *modo update* y el loader termina de escribir (web: INSTALL; CLI: el mismo comando). |
| "FM-1 not found" | Otro cable (de datos), otro puerto USB, cerrá apps MIDI; `fm1_install.py --info` lista los puertos. |
| La identidad no coincide después (exit 6) | Repetí la instalación; si sigue, volvé a la V15 oficial desde el instalador web ("Return to official V15"). |
| El FM-1 no arranca | Mantené **OCT− + OCT+** 2 s al encender (UBOOT) y usá la recuperación por hardware que describe el instalador web. |
| Perdiste canciones/sonidos | Restaurá el backup `.json` desde el Web Studio. |

Códigos de salida del CLI: 0 ok, 1 cancelado, 2 paquete inválido o SHA-256 distinto, 3 FM-1 no encontrado,
4 conexión perdida, 5 timeout, 6 otro modelo / otra identidad.
