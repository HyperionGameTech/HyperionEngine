# Compiling the Engine

Hyperion is a cross-platform 3D game engine written in C++20 using CMake (3.10+) as its build system.
The engine compiles as a shared library (`hyperion`) with the editor and sample app linking against it.

## Required Dependencies

All platforms require the following:

* **CMake** (3.10+) - Project generation and build orchestration.
* **Vulkan SDK** - Required for the Vulkan rendering backend, which is the default on macOS, Android, and Linux. On Apple platforms, the SDK includes MoltenVK which translates Vulkan API calls to Metal. On Windows, DirectX 12 is the default backend and uses the Windows SDK, so the Vulkan SDK is only needed if you build the Vulkan backend.

Platform-specific prerequisites:

| Platform | Additional Requirements |
|----------|------------------------|
| **Windows (MSVC)** | `VCPKG_ROOT` environment variable must be set pointing to a vcpkg installation. vcpkg provides the C++ standard library toolchain. |
| **Windows (Clang)** | Same as MSVC - `VCPKG_ROOT` must be set. The Clang toolchain comes with Visual Studio 2026. |
| **macOS** | Install dependencies via Homebrew: `brew install molten-vk` (or run `Tools/Scripts/InstallDependenciesMac.sh`). |
| **Android** | Android NDK (tested with NDK 29). Ninja build system on PATH. |

## Optional Dependencies

Some dependencies are included as Git submodules. If you cloned without `--recursive`, run:

```bash
git submodule update --init --recursive
```

### DXC - DirectX Shader Compiler (Recommended)

DXC is the shader compiler used for all platforms and rendering backends. All engine shaders are written in HLSL (`.hlsl` / `.hlsli`), and DXC compiles them to the appropriate target (SPIR-V for Vulkan, DXIL for DX12).

DXC headers and libraries are bundled in `External/ThirdParty/Source/dxc/` and `External/ThirdParty/Binaries/<Platform>/`. CMake auto-detects DXC and sets `HYP_DXC=1` when found. Not included in your shipped game as the cook process precompiles all your shaders ahead of time.

### .NET Core (for C# Scripting and Editor UI)

The engine supports C# scripting and the editor UI is built with Avalonia UI on .NET 10.0 (C# 13.0). Requires `HYP_DOTNET=1` to be enabled.

* Headers (`nethost.h`, `hostfxr.h`, `coreclr_delegates.h`) are in `External/ThirdParty/Source/dotnetcore/`.
* Native host library must be present in `External/ThirdParty/Binaries/<Platform>/`:
  * **Windows**: `nethost.lib` + `nethost.dll`
  * **macOS**: `libnethost.a` / `libnethost.dylib` / `libnethost.so`
* The `dotnet` CLI must be on PATH for building the C# projects.

### Git Submodule Dependencies

* **JoltPhysics** - Physics simulation (required; the configure step fails if the submodule is missing).
* **zlib** - Compression/decompression of serialized data.
* **xatlas** - Lightmap UV generation (can be disabled if not needed).
* **tracy** - Tracy profiler client - see [Profiling with Tracy](#profiling-with-tracy).

### Manually Installed Dependencies

* **FreeType** - Text rendering in the editor and engine.
* **OpenAL Soft** - Audio playback.
* **Aftermath** (Windows only) - NVIDIA GPU crash dump analysis. If `GFSDK_Aftermath_Lib.x64.lib` exists in `External/ThirdParty/Binaries/Windows/{Debug|Release}/`, Hyperion links to it.

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

### Generated Project Files

After CMake configuration, generated project files are in the build directory:

| Platform | Build Directory | Project Files |
|----------|----------------|---------------|
| Windows MSVC | `Build/Windows/Release/` | `hyperion.sln` (Visual Studio 2026) |
| Windows Clang | `Build/Windows-Clang/Release/` | `hyperion.sln` (Visual Studio 2026, ClangCL toolset) |
| macOS | `Build/Mac/Release/` | Makefiles (or `hyperion.xcodeproj` if Xcode was requested) |
| Android | `Build/Android/Release/` | Ninja build files |

### Visual Studio / IDE Integration

**In addition to generating .sln files, you can also open the `Source` folder directly in Visual Studio 2026**. VS has native CMake support and will detect `Source/CMakeLists.txt` as the project root. This allows you to configure, build, and debug without generating `.sln` files manually.

For this setup, we support using Clang for Windows as well as Android.

## Platform-Specific Notes

### Windows

- **Rendering backend:** DirectX 12 is a fully supported, mainline backend and is the default on Windows (MSVC and Clang). Vulkan is used on the other platforms.
- **`VCPKG_ROOT` must be set.** The build will fail without it. Point it to your vcpkg installation directory.

### macOS

- Install dependencies: `brew install molten-vk` or run `Tools/Scripts/InstallDependenciesMac.sh`.
- To generate Xcode projects, pass `Xcode` as an argument to the build script.
- **Disable Metal API Validation** when debugging in Xcode: Open scheme settings -> Options tab -> uncheck "Enable Metal API Validation". Since Hyperion uses Vulkan (translated to Metal via MoltenVK), Metal API validation causes issues.

### Android

- Requires Android NDK (tested with NDK 29). Set `ANDROID_NDK` or `ANDROID_NDK_HOME` environment variable, or the script defaults to `%LOCALAPPDATA%\Android\Sdk\ndk\29.0.14206865`. (@TODO: Fix default path)
- Uses Ninja as the build system — ensure `ninja` is on PATH or installed via Visual Studio or Android SDK CMake tools.
- Builds for `arm64-v8a` with `android-28` as the minimum API level.

## Profiling with Tracy

Pass `tracy` to the build script when regenerating CMake (sets `HYP_TRACY=ON`, ignored for shipping and web builds):

```bash
./Tools/Scripts/BuildHyperion.sh Linux Release regenerate tracy
```

`HYP_SCOPE`, `HYP_NAMED_SCOPE`, `ENGINE_STAT_SCOPE` and `HYP_PROFILE_BEGIN` become Tracy CPU zones, and `ENGINE_STAT_GPU_SCOPE` becomes a Tracy GPU zone (Vulkan only for now). The render thread marks the main frame and the sim thread marks a separate `Simulation` frame.

The client only collects data while a profiler is connected. Run the engine, open `tracy-profiler` and connect to it. The profiler must be built from the same version as `External/Submodules/tracy` (v0.14.1; see `External/Submodules/tracy/profiler`), otherwise it will refuse the connection.

### Profiling a game

A game made with **Build Game** links the editor's engine library, and the SDK compiles it with the same Tracy definitions and headers, so its own `HYP_SCOPE`s show up beside the engine's.

Run the built game, or play it in the editor, and connect `tracy-profiler` to it.

**Package Game** doesn't include Tracy (see below).

## Shipping SDK (Package Game)

**Package Game** links the game against a static, `HYP_SHIPPING` build of the engine, never the editor's engine library. That build lives in `Sdk/Shipping` of an editor package, and in `Binaries/<Platform>/ShippingSdk/Sdk` of a source tree. Build it with `sdk`:

```bash
Tools\Scripts\BuildHyperion.bat Release ninja sdk regenerate
./Tools/Scripts/BuildHyperion.sh sdk ninja regenerate
```

`sdk` sets `HYP_SHIPPING=1` and `HYP_SHIPPING_SDK=ON`. That builds every engine library as a static archive without LTO, so any linker can read it, NativeAOT's `link.exe` included. It also writes `Sdk/StaticLibraries.txt`, which lists every archive and system library the engine needs. On Windows the engine and its vcpkg dependencies (the `x64-windows-hyp-static-mt` triplet) are built on the static VC runtime. The game swaps in the dynamic UCRT at link time, as NativeAOT does, so a game's C# scripts can be linked into the same executable.

Games build against it with `-DHYPERION_PACKAGE=ON`. Their module is then an `OBJECT` library linked into the launcher (`HYPERION_GAME_MODULE_TYPE`), and only the libraries the executable imports from the SDK are copied beside it. A project made before this change declares its module `SHARED`. Change it to `add_library(<Name>Game ${HYPERION_GAME_MODULE_TYPE} ...)` in `Source/CMakeLists.txt`; configuring stops with that message until you do.

## Shader Compilation

All engine shaders are written in **HLSL**. The engine uses **DXC** (DirectXShaderCompiler) to compile shaders at build time.

The shader configuration is in `Config/Shaders/Shaders.hmf`, which maps shader names to source files.
If you need to add another shader you can add another entry into this, mapping shader type (e.g PS = "Pixel Shader", VS = "Vertex Shader", CS = "Compute Shader") to filename, relative to the Source/Shaders directory.

> This config is an instance of ShaderDefinitions, you can check out the header in Source/Engine/Rendering/Util/ShaderDefinitions.hpp for the schema.