# Technical Overview — Quantum GammaWave (QG-1)

## 1. Hardware Architecture (M-VAVE FM-1)
- **SoC**: JieLi AC791N (Dual-core 32-bit RISC `pi32v2` architecture, clockable up to 240 MHz).
- **SRAM**: 96 KB total on-chip data RAM.
- **Flash Storage**: 2 MB SPI NOR Flash mapped via XIP (eXecute In Place) at `0x02000000`.
- **Display**: 240×240 IPS RGB565 LCD display driven over high-speed SPI with DMA.
- **User Interface**:
  - 12 silicone front-panel buttons arranged in 2 rows of 6 (`FX`, `SCL`, `ENV`, `LFO`, `EDIT`, `GLO`; `HOME`, `SAVE`, `ARP`, `SEQ`, `PLAY`, `REC`).
  - 2 Octave buttons (`OCT-`, `OCT+`).
  - 27 microswitch keys arranged in chromatic keyboard layout ($F3 \dots G5$).
  - 8 continuous rotary encoders (`MASTER` analog pot, `SELECT`, `PRESETS`, `ALGORITHM`, `KNOB 1`–`KNOB 4`).
- **Audio Codec**: On-chip stereo 24-bit/48 kHz DAC/ADC with class-AB headphone amplifier and line output.

## 2. Firmware Memory Map & Dual-Core Audio Engine
- **Core 0**:
  - Handles USB CDC/MIDI/UAC endpoints, SysEx engine, UI state machine, display rendering DMA, and sequencer ticks.
  - Synthesizes Tracks 1, 2, and 3.
  - Computes the Master Bus (Mix, Punch, Clip, Duck, Limiter, Reverb returns).
- **Core 1**:
  - Dedicated DSP coprocessor.
  - Synthesizes Tracks 4, 5, and 6 (including the DRUM-X multi-pad synthesized drums).
  - Synchronizes via lockless ring buffers and inter-core hardware mailboxes (`fm1_mc.h`).
- **Memory Footprint**:
  - Data RAM allocation (`.data` + `.bss`) capped below 92 KB.
  - Strict minimum pool headroom of 8 KB reserved at `_pool_end` for dynamic project staging buffers.
  - Hot execution paths (`HOT_FNS`) reside in RAM (`.hot_text`) to avoid SPI flash wait states.

## 3. DSP & Audio Synthesis Engines
1. **ANALOG**: Dual virtual-analog anti-aliased oscillators (`SAW`, `SQR`, `TRI`, `SIN`, `PWM`, `SYNC`, `RING`, `SAW3`, `TUBE`, `FOLD`).
2. **FM6**: 6-Operator Yamaha DX7 compatible engine (32 classic algorithms, full sysex import, 32-voice user bank).
3. **PHASE**: Casio CZ-style Phase Distortion synthesis with resonant waveforms and feedback.
4. **LOFI**: 1-bit, 4-bit, and 8-bit vintage chip emulations, plus Bytebeat and Floatbeat algorithmic generators.
5. **VOICE**: Formant vocal synthesizer with vocal tract filter ribbons (`OO`, `OH`, `AH`, `EH`, `EE`).
6. **WHEEL**: Hammond B3 tonewheel drawbar organ simulation with rotary speaker drive and percussion click.
7. **NOISE**: Tuned pitched and free-running metallic noise generators for sweeps and percussion.
8. **DRUM-X**: 8-pad procedural drum synthesizer (`BD`, `SD`, `CP`, `CH`, `OH`, `TM`, `RS`, `CB`) with A/B kit morphing, transient shaper, and producer snare/clap modeling.

## 4. Web Studio & Browser WASM Architecture
- **Web Studio (`web/editor.html`)**:
  - Real-time bidirectional Web MIDI communication (SysEx protocol).
  - 4-Track note pattern tape recorder with bounce, layer, and 44.1 kHz WAV export.
  - Procedural 49-genre pattern generator.
  - Offline mode with automatic fallback when no physical hardware is connected.
- **WebAssembly Emulator (`web/emu/`)**:
  - Compiles the actual C firmware source (`firmware/src/`) using Emscripten (`emcc`) via `web/emu/felucca_web.c`.
  - Audio processing runs in dedicated `AudioWorklet` (`web/emu/worklet.js`) at 44.1 kHz stereo.
  - Canvas renders identical 240×240 RGB565 frames produced by `gfx.c`.
  - Virtual SPI Flash persisted across sessions using browser `localStorage`.
