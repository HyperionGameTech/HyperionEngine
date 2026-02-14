# Compiling the Engine

## Dependencies
Hyperion has a few required dependencies to compile. In addition, there are several optional dependencies that enable additional functionality such as shader compilation, scripting, networking, etc. Let's start with the required dependencies:

### Required dependencies

* CMake - We currently use CMake to generate project files and Makefiles. You'll see multiple CMakeLists.txt files which define how projects are generated.
* (macOS only) MoltenVK - As Hyperion uses a Vulkan rendering backend on Apple platforms, the MoltenVK library is needed in order to translate Vulkan API calls to Metal calls as well as convert SPIR-V shader code to Metal Shading Language.

## Optional* dependencies

The following dependencies are optional for compiling the engine, but without them, engine and editor functionality may be limited and/or missing features.
Some of the dependencies listed below are included as Git submodules. If you cloned the repository with the `--recursive` flag, these submodules should already be pulled. If you didn't, you can still pull them after cloning.
If you cloned the repository without the `--recursive` flag, you can run the following command to pull the submodules:
```bash
git submodule update --init --recursive
```
### Optional dependencies that are included as Git submodules
* [.NET Core](https://github.com/dotnet/runtime) - The .NET Core is used enable C# scripting for gameplay and editor functionality.
* [zlib](https://github.com/madler/zlib) - Hyperion uses zlib to compress and decompress serialized data.
* [glslang](https://github.com/KhronosGroup/glslang) - Used to compile GLSL shaders to SPIR-V and perform shader reflection. We use a version with [some minor modifications](https://github.com/notomorrow/glslang/tree/hyp-modifications)
* [xatlas](https://github.com/jpcy/xatlas) - Used to generate lightmap UVs, can be skipped or disabled if you don't need this.

### Optional dependencies you can install manually
* [Bullet Physics](https://github.com/bulletphysics/bullet3) - Hyperion uses Bullet Physics for physics simulation. This is an optional dependency, but if you want to use physics in your game, you'll need to have it installed for CMake to find it. (Note, implementation is bare bones at the moment)
* [FreeType](https://freetype.org/) - Used for rendering text in the editor and engine.
* [OpenAL Soft](openal-soft.org) - Used for audio playback.
* Aftermath (Windows only) - Provides crash dump information for GPU faults and similar issues. If `GFSDK_Aftermath_Lib.x64.
lib` exists in the `lib/Win32/{Debug|Release}` directory, Hyperion will link to it.

### Experimental (you probably won't need these)
* [libdatachannel](https://libdatachannel.org/) (included as a Git submodule) - For WebRTC support (only needed if you want to use experimental WebRTC streaming).
* [GStreamer](https://gstreamer.freedesktop.org/) - For cloud streaming (only needed if you want to use experimental WebRTC streaming).

## Run the Build Tool

In the project's base directory, you'll find a folder named `codegen` containing source code and CMake files separated from the rest of the engine. This application reads through the engine's header files and parses them to generate code that is used by Hyperion's reflection, serialization, and scripting systems.

codegen is set up to run whenever you reconfigure the engine's CMake files. To invoke it manually, there are two scripts located under tools/scripts:

* `RunCodeGen.bat` (Windows only)
* `RunCodeGen.sh` Linux/macOS/others

Run the appropriate script depending on your platform whenever you want to reprocess files. It's necessary to do this whenever you make changes source code that needs to be seen by the systems mentioned above.

`codegen` links to Hyperion's core library the same way the rest of the engine does, allowing the two separate systems to share a familiar codebase. Therefore, the first time you attempt to compile Hyperion, it will need to compile `core` first before compiling `codegen`. After that, you should be able to configure and compile `hyperion`.

Similiarly to how you can invoke the build tool manually, you can also compile it separately via the following scripts:
* `BuildCodeGen.bat` (Windows only)
* `BuildCodeGen.sh` (macOS / Unix)

Run these scripts from the command line to compile codegen separately from the rest of the engine. This might be useful for debugging purposes or to ensure that any changes in codegen itself are incorporated without needing to recompile the entire engine immediately.

## Compile library / executable

While the engine itself compiles as a dynamic library, the main driver for the engine is an executable file providing (currently limited) editor functionality. This executable links to the hyperion library.

After CMake has been configured and the build tool has been run, you should be able to build the engine itself. Depending on your current platform, CMake will have generated into the build folder:

* Visual Studio Solution files (.sln) (Windows only) - Visual Studio now supports CMake projects directly, so you're able to just open the root directory of the cloned repository. You can also use these generated solution files.
* Makefiles (for Unix based OSes)

## Build Scripts

To make compiling easier, we've also added a `build` script in the root directory of the project that compiles the code for you as well as runs the build tool. There are also more useful build scripts in the `tools/scripts` directory for more fine-grained control over the build process.

## Visual Studio specifics

Note: If you're planning on using Visual Studio, be sure to compile the engine with the configuration set to either `Release` or `RelWithDebInfo`. MSVC's `Debug` configuration adds too much overhead for the engine to run smoothly.

## macOS specifics

On macOS, you will need to install the following dependencies via Homebrew using the `InstallDependenciesMac.sh` script located in the `tools/scripts` directory:
```bash
./tools/scripts/InstallDependenciesMac.sh
```
To generate XCode projects for macOS, you can run the `build.sh` script in the root directory with `--xcode` passed in as a command line argument.

### Important note about Metal API validation
**If you debug with XCode you will need to disable Metal API validation in the scheme settings.** This is because Hyperion uses Vulkan, which is translated to Metal on macOS, and Metal API validation can cause issues with Vulkan calls. To do this, open the scheme settings in XCode, go to the "Options" tab, and uncheck "Enable Metal API Validation".
