# AI Handoff Document — Quantum GammaWave (QG-1)
## Continuity & Transfer of Knowledge for Jules

Welcome, Jules! This document contains all essential context, verified baseline information, and operational guidance to continue developing and maintaining the **Quantum GammaWave (QG-1)** firmware and Web Studio without repeating research or hitting known pitfalls.

---

## 1. Executive Summary & Repository Identity
- **Repository**: `Guillel51/Quantum-GammaWave`
- **Target Hardware**: M-VAVE FM-1 Groovebox / Multitrack Synthesizer (dual-core JieLi AC791N SoC).
- **Architecture**:
  - Embedded C/Asm firmware in `firmware/src/` & `firmware/hal/`
  - Browser WebAssembly emulator in `web/emu/`
  - Web Studio workstation & USB flasher in `web/editor.html` & `web/index_pkg.html`
  - Python build and generator tooling in `tools/`
  - Comprehensive unit test and simulator suite in `tests/`

---

## 2. Commit Baseline & Current Branch
- **Working Branch**: `feature/quantum-handoff-evolution`
- **Base Commit**: `dff422e3e65ba5df7b4ca0700bf90f6a9c80c1f7` (Merge PR #2 `Guillel51/dev/quantum-integral`)
- **Current Commit**: `9cec8b163990fcbf8cde8f8e75298b8f7eeb50d3`
- **Key Files Created / Maintained**:
  - `AGENTS.md`: Full operating instructions, hardware safety directives, build commands.
  - `docs/PROJECT_STATUS.md`: Up-to-date test verification and subsystem status.
  - `docs/TECHNICAL_OVERVIEW.md`: Detailed DSP, dual-core, memory map, and Web Studio architecture.
  - `docs/ROADMAP.md`: Feasibility analysis and memory budgets for Peak/Notch EQ, Tape Warmth, Bit Freeze.
  - `docs/HARDWARE_TEST_PLAN.md`: Protocol for physical hardware testing on real FM-1 units.
  - `launch_emulator.bat`, `launch_studio.bat`, `launch_browser_installer.bat`: Python path auto-detection and server launchers.

---

## 3. How to Run Verifications Quickly
Always verify changes using the fast local test sequence:
```bash
# 1. Regenerate firmware tables and headers
python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"

# 2. Host installer CLI simulation tests
python3 tests/install_test.py

# 3. Web Studio & backup protocol tests
node web/test_web.mjs
node web/test_backup.mjs
node web/test_pkg_bounds.mjs
```

---

## 4. Key Rules to Remember
1. **Never physical-flash real hardware**: All testing in automated environments is host-simulated or WebAssembly-based.
2. **Preserve memory budget**: Keep SRAM data usage under 92 KB, leaving $\ge 8\text{ KB}$ headroom in `POOL_END - pool_top`.
3. **Keep all licenses & credits**: Preserve GPL-3.0 and original attributions to Leo Kuroshita / Hügelton Instruments, JIANT FM, and Felucca.
4. **Follow pre-commit steps**: Always use `pre_commit_instructions` before submitting work.
