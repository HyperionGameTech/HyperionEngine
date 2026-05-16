# Compiling the Engine

Hyperion is a cross-platform 3D game engine written in C++20 using CMake (3.10+) as its build system.
The engine compiles as a shared library (`hyperion`) with the editor and sample app linking against it.

## Required Dependencies

All platforms require the following:

* **CMake** (3.10+) - Project generation and build orchestration.
* **Vulkan SDK** - Required for the Vulkan rendering backend (the default and primary backend). On Apple platforms, the SDK includes MoltenVK which translates Vulkan API calls to Metal.

Platform-specific prerequisites:

| Platform | Additional Requirements |
|----------|------------------------|
| **Windows (MSVC)** | `VCPKG_ROOT` environment variable must be set pointing to a vcpkg installation. vcpkg provides the C++ standard library toolchain. |
| **Windows (Clang)** | Same as MSVC - `VCPKG_ROOT` must be set. The Clang toolchain comes with Visual Studio 2026. |
| **macOS** | Install dependencies via Homebrew: `brew install bullet molten-vk` (or run `Tools/Scripts/InstallDependenciesMac.sh`). |
| **Android** | Android NDK (tested with NDK 29). Ninja build system on PATH. |

## Optional Dependencies

Some dependencies are included as Git submodules. If you cloned without `--recursive`, run:

```bash
git submodule update --init --recursive
```

### DXC - DirectX Shader Compiler (Recommended)

DXC is the shader compiler used for all platforms and rendering backends. All engine shaders are written in HLSL (`.hlsl` / `.hlsli`), and DXC compiles them to the appropriate target (SPIR-V for Vulkan, DXIL for DX12).

DXC headers and libraries are bundled in `External/ThirdParty/Source/dxc/` and `External/ThirdParty/Binaries/<Platform>/`. CMake auto-detects DXC and sets `HYP_DXC=1` when found.

### .NET Core (for C# Scripting and Editor UI)

The engine supports C# scripting and the editor UI is built with Avalonia UI on .NET 9.0 (C# 13.0). Requires `HYP_DOTNET=1` to be enabled.

* Headers (`nethost.h`, `hostfxr.h`, `coreclr_delegates.h`) are in `External/ThirdParty/Source/dotnetcore/`.
* Native host library must be present in `External/ThirdParty/Binaries/<Platform>/`:
  * **Windows**: `nethost.lib` + `nethost.dll`
  * **macOS**: `libnethost.a` / `libnethost.dylib` / `libnethost.so`
* The `dotnet` CLI must be on PATH for building the C# projects.

### Git Submodule Dependencies

* **zlib** - Compression/decompression of serialized data.
* **xatlas** - Lightmap UV generation (can be disabled if not needed).

### Manually Installed Dependencies

* **Bullet Physics** - Physics simulation. Install via Homebrew (`brew install bullet`) on macOS, or ensure CMake can find it on other platforms.
* **FreeType** - Text rendering in the editor and engine.
* **OpenAL Soft** - Audio playback.
* **Aftermath** (Windows only) - NVIDIA GPU crash dump analysis. If `GFSDK_Aftermath_Lib.x64.lib` exists in `External/ThirdParty/Binaries/Windows/{Debug|Release}/`, Hyperion links to it.

### Experimental

* **libdatachannel** (Git submodule) - WebRTC support.
* **GStreamer** - Cloud streaming (for WebRTC streaming).

## Build Tool (CodeGen)

The engine uses a custom tool that parses engine headers to generate reflection, serialization, and scripting code. This tool lives in `Tools/CodeGen/` and is automatically invoked during CMake configuration via `RunCodeGen(<major version> <minor version>)`.

To run it manually:

**Windows:**
```batch
Tools\Scripts\RunCodeGen.bat <major version> <minor version>
```

**macOS/Linux:**
```bash
./Tools/Scripts/RunCodeGen.sh <major version> <minor version>
```

The version arguments (`0 9`) are the major.minor version. If the codegen binary is out of date, the script automatically rebuilds it via `BuildCodeGen.bat` / `BuildCodeGen.sh`.

**Note:** CodeGen must succeed before the engine will compile. Output is written to `Source/Generated/` (C++) and `Source/Generated/CSharp/` — do not edit generated files manually.

## Building

### Quick Build (All Platforms)

Use the root-level convenience scripts:

**Windows:**
```batch
build.bat Release              # MSVC, Release
build.bat Release Clang        # ClangCL, Release
build.bat Debug                # MSVC, Debug
build.bat Debug Clang          # ClangCL, Debug
```

**macOS:**
```bash
./build.sh Release             # Default (Makefiles)
./build.sh Debug
./build.sh Xcode Release       # Generate Xcode project
```

### Fine-Grained Build Scripts

Use `Tools/Scripts/BuildHyperion.bat` (Windows) or `Tools/Scripts/BuildHyperion.sh` (macOS/Linux) for more control.

**Windows (`BuildHyperion.bat`):**

Supports the following arguments (order-independent):

| Argument | Effect |
|----------|--------|
| `Release` | Build type Release (default) |
| `Debug` | Build type Debug |
| `Clang` | Use ClangCL toolchain (VS 2026) |
| `Android` | Build for Android ARM64 |
| `regenerate` | Force CMake re-generation |
| `nowait` | Skip the 3-second CMake prompt, used mainly for automation. |


**macOS/Linux (`BuildHyperion.sh`):**

| Argument | Effect |
|----------|--------|
| `Release` | Build type Release (default) |
| `Debug` | Build type Debug |
| `Xcode` | Generate Xcode project |
| `iOS` | Cross-compile for iOS device |
| `iOSSimulator` | Cross-compile for iOS simulator |

### Generated Project Files

After CMake configuration, generated project files are in the build directory:

| Platform | Build Directory | Project Files |
|----------|----------------|---------------|
| Windows MSVC | `Build/Windows/Release/` | `hyperion.sln` (Visual Studio 2026) |
| Windows Clang | `Build/Windows-Clang/Release/` | `hyperion.sln` (Visual Studio 2026, ClangCL toolset) |
| macOS | `Build/Mac/Release/` | Makefiles (or `hyperion.xcodeproj` if Xcode was requested) |
| Android | `Build/Android/Release/` | Ninja build files |

### Visual Studio / IDE Integration

**Open the `Source` folder directly in Visual Studio 2026** — VS has native CMake support and will detect `Source/CMakeLists.txt` as the project root. This allows you to configure, build, and debug without generating `.sln` files manually.

To use ClangCL in Visual Studio's CMake integration, go to **Project > CMake Settings** and set the toolset to `ClangCL`.

Engine output binaries are placed in `Binaries/{Platform}/{BuildType}/`.

## Platform-Specific Notes

### Windows

- **`VCPKG_ROOT` must be set.** The build will fail without it. Point it to your vcpkg installation directory.
- **Use Release or RelWithDebInfo**, not Debug. MSVC Debug builds have excessive overhead that prevents the engine from running smoothly.
- The MSVC generator is **Visual Studio 18 2026** targeting **x64**.

### macOS

- Install dependencies: `brew install bullet molten-vk` or run `Tools/Scripts/InstallDependenciesMac.sh`.
- To generate Xcode projects, pass `Xcode` as an argument to the build script.
- **Disable Metal API Validation** when debugging in Xcode: Open scheme settings -> Options tab -> uncheck "Enable Metal API Validation". Since Hyperion uses Vulkan (translated to Metal via MoltenVK), Metal API validation causes issues.

### Android

- Requires Android NDK (tested with NDK 29). Set `ANDROID_NDK` or `ANDROID_NDK_HOME` environment variable, or the script defaults to `%LOCALAPPDATA%\Android\Sdk\ndk\29.0.14206865`.
- Uses Ninja as the build system — ensure `ninja` is on PATH or installed via Visual Studio or Android SDK CMake tools.
- Builds for `arm64-v8a` with `android-28` as the minimum API level.

### iOS

- Requires `VULKAN_SDK` environment variable.
- Builds target iOS 14.0+ and use MoltenVK for Vulkan to Metal translation.

## Shader Compilation

All engine shaders are written in **HLSL**. The engine uses **DXC** (DirectXShaderCompiler) to compile shaders at build time.

* **Vulkan**: HLSL -> SPIR-V (via DXC's `-spirv` target)
* **DX12**: HLSL -> DXIL (via DXC's default target)

The shader configuration is in `Source/Shaders/Shaders.ini`, which maps shader names to source files and defines compile-time permutations (e.g., `INSTANCING`, `HAS_PHYSICS`).

## Output Structure

```
Binaries/
├── Windows/
│   ├── Release/
│   │   ├── hyperion.dll        # Main engine library
│   │   ├── hyperion-core.dll
│   │   └── ...
│   └── Debug/
├── Mac/
│   ├── Release/
│   │   ├── libhyperion.dylib
│   │   └── ...
├── Android/
└── iOS/
```
