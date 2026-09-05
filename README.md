# Open-X DSP Audio Suite

[![CI/CD](https://github.com/AA-EION/Open-X/actions/workflows/build.yml/badge.svg)](https://github.com/AA-EION/Open-X/actions/workflows/build.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++20](https://img.shields.io/badge/Standard-C%2B%2B20-orange.svg)](https://en.cppreference.com/w/cpp/20)
[![JUCE 8](https://img.shields.io/badge/Framework-JUCE%208-green.svg)](https://juce.com/)
[![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Windows-lightgrey.svg)](#platform-support)
[![Architecture](https://img.shields.io/badge/Architecture-Universal%20(ARM64%20%2B%20x86__64)-purple.svg)](#universal-binaries)

**Open-X DSP** is a collection of 7 audio processing plugins built with modern C++20 and JUCE 8. Engineered for zero-allocation real-time performance, Open-X combines digital signal processing algorithms with interactive, GPU-accelerated visualizations.

---

## Plugin Suite Overview

| Plugin | Category | Key DSP Capabilities |
| :--- | :--- | :--- |
| **EQ-X** | Dynamic Parametric EQ | Biquad filter topologies, interactive frequency curve, FFT spectrum analyzer, proportional-Q dynamic expansion/compression |
| **Comp-X** | Studio Compressor | Peak / RMS detection, lookahead buffer, variable topology, sidechain high-pass/band-pass filter, real-time dynamics scope |
| **Limit-X** | True-Peak Brickwall Limiter | 4x inter-sample true-peak detector, smooth lookahead gain reduction window, transparent clipping prevention |
| **Verb-X** | Algorithmic FDN Reverb | 16-delay-line Feedback Delay Network, Householder orthogonal rotation matrix, high-frequency damping, stereo spatial diffusion |
| **MB-X** | Multiband Dynamics | 4-band phase-matched Linkwitz-Riley (LR4) crossover networks, independent band threshold/ratio/attack/release |
| **DS-X** | Spectral De-Esser | Targeted sibilance detection filter, transparent dynamic attenuation, sidechain delta monitoring mode |
| **Gate-X** | Predictive Noise Gate | Predictive lookahead detector, hysteresis opening/closing thresholds, transient preservation hold stage |

---

## Key Architectural Highlights

- **Universal Multi-Architecture Support**:
  - **macOS**: Built as universal fat Mach-O binaries (`arm64` + `x86_64`) running natively on Apple Silicon (M1/M2/M3/M4) and Intel Macs.
  - **Windows**: Packaged as universal VST3 multi-architecture bundles (`Contents/x86_64-win` and `Contents/arm64-win`) running natively on AMD64 and ARM64 Windows PCs.
- **Ad-Hoc Signing with Hardened Runtime**:
  - All macOS plugins, components, and GUI applications are signed ad-hoc with hardened runtime (`codesign --force --deep -s - --options=runtime`) for Gatekeeper compatibility.
- **Single Bundle Deployment**:
  - A single installer for macOS (`Open-X-Suite-macOS.pkg`) and Windows (`Open-X-Suite-Windows.msi`) deploys the complete suite.
- **Dedicated Clean GUI Uninstaller**:
  - macOS installer places `Open-X Uninstaller.app` in `/Applications`. The uninstaller scans installed suite components, wipes user/system files, purges package receipts (`pkgutil --forget`), clears DAW plugin caches (`AudioComponentRegistrar`), and automatically deletes itself upon completion.
  - Windows MSI registers full bundle uninstallation with Windows Add/Remove Programs (Installed Apps) and places a Start Menu uninstaller shortcut (`msiexec /x`).

---

## Repository Structure

```
Open-X/
├── cmake/                                # Shared CMake macros and plugin declarations
├── modules/
│   ├── openx_dsp/                        # C++20 DSP engines (EQ, Dynamics, Reverb, Crossover)
│   └── openx_ui/                         # Vector UI system, OpenXLookAndFeel, curves, scopes
├── plugins/
│   ├── Comp-X/                           # Studio Compressor
│   ├── DS-X/                             # Spectral De-Esser
│   ├── EQ-X/                             # Dynamic Parametric Equalizer
│   ├── Gate-X/                           # Predictive Noise Gate
│   ├── Limit-X/                          # True-Peak Limiter
│   ├── MB-X/                             # 4-Band Multiband Processor
│   └── Verb-X/                           # Algorithmic FDN Reverb
├── uninstaller/                          # Dedicated macOS GUI Uninstaller application
│   ├── CMakeLists.txt                    # juce_add_gui_app(OpenXUninstaller ...)
│   └── source/                           # JUCE UI and privileged AppleScript execution
├── packaging/
│   ├── macos/                            # packager_macOS.py and component scripts
│   ├── windows/                          # packager_Windows_WiX.py and WiX v5 configurations
│   └── resources/                        # GPL-3 license RTF and installer branding
├── tests/                                # Catch2 / CTest DSP verification unit tests
└── .github/workflows/build.yml           # CI/CD runners building universal installers & artifacts
```

---

## Installation

### macOS (`.pkg` Bundle)
1. Download `Open-X-Suite-macOS.pkg` from GitHub Releases / CI Artifacts.
2. Double-click the installer and follow the wizard.
3. The installer deploys:
   - VST3 plugins to `/Library/Audio/Plug-Ins/VST3/`
   - AudioUnit (AU) plugins to `/Library/Audio/Plug-Ins/Components/`
   - Uninstaller to `/Applications/Open-X Uninstaller.app`
4. Restart your DAW (Logic Pro, Ableton Live, Reaper, Cubase, Studio One, Bitwig, etc.).

### Windows (`.msi` Bundle)
1. Download `Open-X-Suite-Windows.msi` from GitHub Releases / CI Artifacts.
2. Run the MSI setup wizard.
3. The installer deploys all 7 VST3 plugins to `C:\Program Files\Common Files\VST3\`.
4. Rescan plugins in your DAW.

---

## Uninstallation

### macOS
Launch **Open-X Uninstaller** from your `/Applications` folder:
1. Review the detected Open-X suite components.
2. Click **Uninstall Open-X Suite**.
3. Confirm administrator privileges in the macOS prompt.
4. The uninstaller removes:
   - All 7 VST3 and AU plugins
   - Application Support presets and cache (`~/Library/Application Support/Open-X`)
   - Preference files (`~/Library/Preferences/com.openxdsp.*`)
   - System installer package receipts (`pkgutil --forget`)
   - Resets the macOS audio plugin cache (`AudioComponentRegistrar`)
   - Self-deletes `/Applications/Open-X Uninstaller.app` upon completion.

### Windows
Open **Windows Settings** > **Apps** > **Installed Apps** (or Start Menu > **Open-X DSP** > **Uninstall Open-X DSP Suite**):
1. Click **Uninstall**.
2. Windows Installer cleanly removes all plugins, directory structures, and registry keys.

---

## Building From Source

### Prerequisites
- **CMake**: Version 3.24 or higher
- **C++ Compiler**: Modern C++20 compliant compiler:
  - macOS: Apple Clang / Xcode 15+
  - Windows: Visual Studio 2022 (MSVC v143)
- **Ninja** (optional, recommended for fast builds)
- **WiX Toolset v5** (for Windows MSI packaging): `dotnet tool install --global wix --version 5.0.2`

### macOS (Universal Binary Build)
```bash
# Configure for both Apple Silicon (arm64) and Intel (x86_64)
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DOPENX_BUILD_VST3=ON \
  -DOPENX_BUILD_AU=ON \
  -DOPENX_BUILD_UNINSTALLER=ON

# Compile plugins and uninstaller
cmake --build build --config Release --parallel

# Run unit tests
ctest --test-dir build -C Release --output-on-failure

# Package universal PKG installer
python3 packaging/macos/packager_macOS.py
```

### Windows (MSI Build)
```powershell
# Configure with Visual Studio 2022
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DOPENX_BUILD_VST3=ON `
  -DOPENX_ENABLE_TESTS=ON

# Compile suite
cmake --build build --config Release --parallel

# Run unit tests
ctest --test-dir build -C Release --output-on-failure

# Generate WiX manifest and build MSI
python packaging/windows/packager_Windows_WiX.py
```

---

## Continuous Integration & Delivery

The GitHub Actions workflow (`.github/workflows/build.yml`) runs on every push and pull request:
- **macOS-ARM64 Runner (`macos-14`)**:
  - Compiles universal `arm64;x86_64` plugins and the uninstaller application.
  - Ad-hoc signs all Mach-O binaries with hardened runtime.
  - Builds and packages `Open-X-Suite-macOS.pkg`.
  - Publishes `OpenX-Suite-macOS-Installer` and `OpenX-Uninstaller-macOS` artifacts.
- **Windows-x86_64 Runner (`windows-2022`)**:
  - Compiles VST3 plugins with MSVC.
  - Installs WiX 5.0.2 and WiX UI extension.
  - Generates WiX manifest and compiles `Open-X-Suite-Windows.msi`.
  - Publishes `OpenX-Suite-Windows-Installer` artifact.

---

## License

Open-X DSP Audio Suite is licensed under the **GNU General Public License v3.0 (GPLv3)**.  
See the [LICENSE](LICENSE) file for full terms and conditions.

Copyright (C) 2026 Open-X DSP Project Contributors.
