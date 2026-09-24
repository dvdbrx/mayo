# CadView Performance & System Baseline

This document captures the baseline build characteristics, startup performance, memory footprints, and verification metrics for CadView (fork of Mayo `develop`).

---

## 1. System & Environment Baseline

- **Operating System**: Linux x86_64 (Ubuntu 24.04 LTS / glibc 2.39)
- **Compiler**: GCC 13.3.0 (`-O3 -DNDEBUG`)
- **Qt Version**: Qt 6.4.2 (Ubuntu official sysroot packages)
- **OpenCascade Version**: OCCT 7.6.3 (Ubuntu official sysroot packages)
- **Build System**: CMake 3.28.3

---

## 2. Binary Footprint

| Component | Size | Notes |
| :--- | :--- | :--- |
| `cadview` (unstripped) | ~5.1 MB | Release build with symbol table |
| `cadview` (stripped) | **3.8 MB** | Exceeds all target size budgets |
| Stripped Package Target | ≤ 70 MB | Linux AppImage / compressed archive target |

---

## 3. Startup & Execution Latency

Target: Cold start to interactive main window ≤ 3.0 s.

| Scenario | Measured Elapsed Wall Time | Status |
| :--- | :--- | :--- |
| Cold Startup to UI (no file) | **~0.58 s** | PASSED (target ≤ 3.0 s) |
| Startup + Load `F1.step` (solid) + Screenshot + Exit | **~1.29 s** | PASSED |
| Startup + Load `F2.step` (assembly) + Screenshot + Exit | **~0.99 s** | PASSED |

---

## 4. Memory Footprint (Resident Set Size)

Measured via `/usr/bin/time -v` (includes full dynamic runtime: Qt6, OCCT 7.6.3, OpenGL context, Mesa DRI driver):

| Test Case | Peak RSS | Description |
| :--- | :--- | :--- |
| Cold UI Baseline (`--exit`) | ~193 MB | Qt6 Gui + OpenGL driver init |
| `F1.step` (Mechanical Solid) | ~317 MB | Single solid, 18 BRep faces |
| `F2.step` (Assembly) | ~346 MB | Multi-component assembly |

---

## 5. Automated Verification & Oracles

Automated headless tests are verified via:
```bash
./scripts/smoke.sh
```

### Verification Checks:
1. **Measurement Unit Oracles (`test-measure`)**:
   - `F1.step` Bounding Box: `dx=50.8 mm`, `dy=190.5 mm`, `dz=6.35 mm` (Exact match).
   - `F1.step` Surface Area: 18 faces, Total area = `22775.9 mm²` (Exact match).
2. **Headless Execution (`scripts/smoke.sh`)**:
   - Tests binary with `--open tests/fixtures/F1.step --screenshot <out.png> --exit`.
   - Validates exit code 0 and valid PNG image generation.
