# Build and Toolchain Configuration Guide

**Project:** Industrial Worker Safety Monitoring and Emergency Alert System  
**Target:** NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)  
**SDK Architecture:** MCUXpresso SDK Freestanding Project  

---

## 1. Prerequisites

**Project workflow:** this team uses the **MCUXpresso extension for VS Code** to compile and flash the firmware. Use the extension's project/Quickstart view to build the `FRDM_RW612_WorkerSafety` project, then start its debug/download action with the board connected to MCU-Link USB J10. Use the extension Serial Monitor on the MCU-Link VCOM port at 115200 baud. The command-line CMake instructions below are a fallback for maintainers and diagnostics; they are not the team's normal compile/flash workflow. For the extension workflow, follow [NXP's FRDM-RW612 getting-started guide](https://www.nxp.com/document/guide/getting-started-with-frdm-rw612:GS-FRDM-RW612).

The developer machine still needs the RW612 MCUXpresso SDK, Arm GNU toolchain, and compatible debug/download tools installed and selected by the extension. Dashboard-only Python validation does not require these embedded packages.

To build this project for the FRDM-RW612 microcontroller, you need:

1. **NXP MCUXpresso SDK for RW612** (SDK version 2.14+ / MCUX SDK repository):
   - Example local SDK path: `D:/MCUXpresso/Example/matter_mcuxpresso/mcuxsdk/mcuxsdk`
   - Must contain `cmake/extension/mcux.cmake` and device definition for `RW612ETA2I`.
2. **Arm GNU Toolchain (`arm-none-eabi-gcc`)**:
   - Version 13.x or 14.x (e.g. `arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi`).
3. **CMake** (version 3.22.0 or newer).
4. **Ninja** build tool (version 1.11 or newer).

---

## 2. Environment Variables Setup

Configure the following environment variables before invoking CMake:

### Windows (PowerShell)
```powershell
# Set path to the MCUXpresso SDK root directory (the folder containing cmake/extension/mcux.cmake)
$env:MCUX_SDK_PATH = "D:/MCUXpresso/Example/matter_mcuxpresso/mcuxsdk/mcuxsdk"

# Set path to the Arm GNU Toolchain root directory
$env:ARMGCC_DIR = "C:/path/to/arm-gnu-toolchain"

# Add compiler and ninja to PATH
$env:PATH = "$env:ARMGCC_DIR/bin;$env:PATH"
```

### Linux / macOS (Bash)
```bash
export MCUX_SDK_PATH="/path/to/mcuxsdk"
export ARMGCC_DIR="/path/to/arm-gnu-toolchain"
export PATH="$ARMGCC_DIR/bin:$PATH"
```

---

## 3. Building via CMake Presets

The project includes standard `CMakePresets.json` presets (`debug` and `release`) that pass:
- `SdkRootDirPath`: configured from `$env{MCUX_SDK_PATH}`
- `board`: `frdmrw612`
- `CONFIG_TOOLCHAIN`: `armgcc`

### Debug Build
```powershell
# Configure
cmake --preset debug

# Build
cmake --build --preset debug
```

### Release Build
```powershell
# Configure
cmake --preset release

# Build
cmake --build --preset release
```

### Direct CMake Command (Without Presets)
```powershell
cmake -B build/debug -G Ninja `
  -DSdkRootDirPath="$env:MCUX_SDK_PATH" `
  -Dboard=frdmrw612 `
  -DCONFIG_TOOLCHAIN=armgcc `
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/debug
```

The resulting binaries will be placed in:
- `build/debug/FRDM_RW612_WorkerSafety.elf`
- `build/debug/FRDM_RW612_WorkerSafety.bin`

---

## 4. Flashing the Board

Connect the FRDM-RW612 board via the **MCU-Link USB port (J10)** to your PC.

### Flashing via J-Link / pyOCD / LinkServer
```powershell
# Using pyOCD
pyocd flash -t rw612 build/debug/FRDM_RW612_WorkerSafety.bin --base-address 0x08001000

# Or drag-and-drop the .bin file to the MCU-Link Mass Storage drive if enabled.
```

---

## 5. Collaborator & Multi-Machine Portability

- `CMakePresets.json` uses environment variables `$env{MCUX_SDK_PATH}` and `$env{ARMGCC_DIR}` so no machine-specific user paths are committed.
- Machine-specific files (e.g. `mcux_include.local.json`, `app_secrets.h`) are git-ignored.
- To customize Wi-Fi or local secrets, copy `source/app_secrets.h.example` to `source/app_secrets.h`.
