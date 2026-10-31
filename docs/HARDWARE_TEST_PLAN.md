# Hardware Test Plan — M-VAVE FM-1 Synthesizer & Groovebox

This test plan defines the mandatory physical verification procedure before validating production firmware updates on real M-VAVE FM-1 units.

> **CRITICAL SAFETY DIRECTIVE**: Simulated environments and CI runners cannot confirm physical hardware health. Never state that hardware tests pass unless verified on a physical device.

---

## 1. Power-On & Bootloader Verification
- [ ] Connect FM-1 to regulated USB-C 5V power supply.
- [ ] Turn power switch ON.
- [ ] Verify OLED/IPS display initializes within 1.5 seconds without screen tearing or white flashes.
- [ ] Verify splash screen: Hearth sun logo, `QUANTUM GAMMAWAVE (QG-1)` branding, version string.
- [ ] Hold `REC` at power-on: confirm device boots into single-core mode with visual prompt on LCD.

## 2. Controls & Matrix Interactivity
- [ ] **12 Panel Buttons**: Tap each button (`FX`, `SCL`, `ENV`, `LFO`, `EDIT`, `GLO`, `HOME`, `SAVE`, `ARP`, `SEQ`, `PLAY`, `REC`). Verify instant UI navigation and corresponding LED backlight status.
- [ ] **Octave Shift**: Press `OCT-` and `OCT+`. Verify transpose changes and LED breathing animation.
- [ ] **Keyboard (27 Keys)**: Play chromatic sequence $F3$ through $G5$. Confirm zero skipped notes, clean trigger velocity response, and LED slit lighting.
- [ ] **8 Rotary Encoders**:
  - `MASTER`: Sweep 0% to 100%. Verify smooth exponential volume taper without zipper noise.
  - `SELECT`, `PRESETS`, `ALGORITHM`: Rotate clockwise/counter-clockwise 20 detents. Verify zero missed ticks.
  - `KNOB 1`–`KNOB 4`: Rotate across active parameters; verify on-screen card indicators reflect exact values.

## 3. Audio & DSP Sound Checks
- [ ] Connect 3.5mm stereo headphones to phone jack.
- [ ] Audition factory presets across all 8 synthesis engines:
  - `ANALOG`: Check dual-oscillator sync, PWM, and new TUBE/FOLD saturation modes.
  - `FM6`: Check operator carriers, feedback loop, and user bank $B1$–$B11$.
  - `PHASE`, `LOFI`, `VOICE`, `WHEEL`, `NOISE`: Check audio output for absence of clicks or DC drift.
  - `DRUM-X`: Check 8 pads, A/B morphing, producer SNARE and CLAP levels.
- [ ] Master bus: Engage `CLIP`, `PUNCH`, and `DUCK`. Verify auto-leveler keeps output clean and pump effect syncs with kick drum.

## 4. Connectivity & Web Studio Communication
- [ ] Connect USB-C cable to PC (Windows 11 / macOS / Linux).
- [ ] Open Web Studio in Chrome/Edge (`http://localhost:8765/web/editor.html` or live site).
- [ ] Verify Web MIDI device recognized as `QUANTUM GAMMAWAVE (QG-1)` or `M-VAVE FM-1`.
- [ ] Send SysEx dump request (`INFO`): verify device parameters synchronize seamlessly.
- [ ] Edit a sound parameter on screen: verify FM-1 LCD updates in real time.
- [ ] Export 16-bit 44.1 kHz WAV file: confirm clean render without dropouts.
