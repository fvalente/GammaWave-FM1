# Roadmap & DSP Feasibility — Quantum GammaWave (QG-1)

This roadmap details the planned evolutions for Quantum GammaWave, including memory, CPU, and algorithmic feasibility calculations.

---

## 1. Planned Firmware & DSP Features

### A. Parametric EQ: Peak/Bell & Notch Filter Models
- **Description**: Add 2nd-order biquad Peak/Bell and Notch filter modes to `EDIT > FILTER` or track insert EQ.
- **Feasibility & Resource Calculation**:
  - *SRAM*: 5 biquad coefficients ($b_0, b_1, b_2, a_1, a_2$) + 2 history state variables per channel ($x_{n-1}, x_{n-2}, y_{n-1}, y_{n-2}$) = 28 bytes per track.
  - *CPU*: Direct Form II Transposed requires 4 multiplications and 4 additions per sample. At 44.1 kHz, this consumes $\approx 0.35\ \text{MIPS}$ per track, easily fitting into Core 0/1 headroom.
  - *Status*: Recommended for Next Development Cycle.

### B. TAPE WARMTH: Saturation, Hiss & Wow/Flutter
- **Description**: Analog tape saturation curve combined with subtle pitch wobble (wow/flutter) and band-limited noise floor on the master bus.
- **Feasibility & Resource Calculation**:
  - *SRAM*: Modulated short delay line of 256 samples $\times$ 2 channels $\times$ 16-bit = 1024 bytes. Fits within spare pool allocation without exceeding 96 KB SRAM limit.
  - *CPU*: Soft tanh/polynomial curve (Pade approximant or cubic Hermite spline) requires 3 MAC operations per sample. LFO modulation requires 1 sine evaluation per block.
  - *Status*: Highly viable. Prototype in Web Studio simulator first.

### C. Bit Freeze & Buffer Granulator
- **Description**: Real-time buffer freeze punch-in effect triggered via `[FX] + Key`.
- **Feasibility & Resource Calculation**:
  - *SRAM*: A 1/8th bar buffer at 120 BPM requires $0.25\ \text{seconds} \times 44100 \approx 11025$ samples. At 16-bit mono, this requires 22 KB of RAM, which exceeds available free SRAM if retained alongside the project pool.
  - *Alternative Implementation*: Downsample frozen grain to 22.05 kHz 8-bit mono $\implies 2.75\text{ KB}$, which safely fits in temporary scratch memory (`pool_top`).
  - *Status*: Viable using low-fi grain buffer alternative.

### D. Pitch Glitch & Stutter Punch-In FX
- **Description**: Instant momentary repitching ($\pm 12$ semitones) and $1/16$ / $1/32$ / triplet stutter bursts.
- **Feasibility & Resource Calculation**:
  - Reuses existing punch-in FX framework (`firmware/src/pfx.c`).
  - Variable-rate playback pointer over circular 512-sample buffer ($1\text{ KB}$ SRAM).
  - *Status*: Low risk, high musical value.

---

## 2. Web Studio & UX Evolutions
- **High-Resolution Vector UI**: Modernize Web Studio skin with responsive SVG knobs and CSS glassmorphism.
- **Stereo VU Meters**: Real-time RMS/peak level indicators on master bus and individual tracks in the Web Studio HUD.
- **Patch Librarian Enhancements**: Batch export/import of 32-voice DX7 `.syx` files directly to and from device user banks.

---

## 3. Prioritized Implementation Order
1. **Milestone 1**: Implement Peak/Notch EQ biquads in `firmware/src/voice.c` and `firmware/src/fx.c`.
2. **Milestone 2**: Implement Tape Warmth saturation algorithm on master mix bus (`firmware/src/master.c`).
3. **Milestone 3**: Add Pitch Glitch & Stutter modes to Punch-in FX matrix.
4. **Milestone 4**: Web Studio UI overhaul with stereo VU metering and animated scopes.
