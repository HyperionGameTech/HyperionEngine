### Hyperion Engine is an easy to use, high performance 3D game engine written in C++ and C# with a Vulkan rendering backend and a visual editor built with Avalonia.

![Hyperion Engine screenshot2](/Documentation/Images/EditorPT.png)

## Features
- Real time lighting and global illumination. (Screen-Space Global Illumination (SSGI), Dynamic Diffuse Global Illumination (DDGI) and ray traced reflections)
- Clustered deferred rendering to support many lights on screen at once.
- Path tracer to use as a reference for lighting scenes.
- Baking system integrated into the editor. Bakes lightmap volumes with path traced lighting, reflection probes, fog volumes, and other static lighting data.
- Cascaded shadow maps, screen space ambient occlusion (SSAO), screen space reflections (SSR), and more.
- Level streaming and world partitioning system to enable efficient memory usage and larger worlds
- Scripting via C# or our custom scripting language, HypScript.
- Hot reloading for shaders and scripts to speed up development time.

## Platforms
Currently, we are focusing our efforts on developing the engine for *Windows*, *macOS* and *Android*. Editor support is available on Windows and macOS.
> Linux support is planned for the future but not in active development. Contributions welcome on that front if you are interested in that!

The engine is in early access and is not yet ready for production use. But if you are feeling adventurous, we welcome you to try it out and see what you can create (or break)!

To get started, check out the [Compiling the Engine](Documentation/CompilingTheEngine.md) guide to set up your development environment and compile the engine.

## Contributing

If you want to contribute please feel free to submit a pull request! However if you do use AI tools to assist you in your development, please do disclose this in your pull request and ensure that the majority of the contribution is your own work.

## Getting started
[Definitions and Terminology](Documentation/Definitions.md) provides definitions and explanations for various terms and concepts used within the engine.

## Console commands and console variables (CVars)

[Console Commands](Documentation/Console.md) are commands that can be executed in the editor's console window to perform various actions or set global states that the engine can use to modify rendering, physics, gameplay, etc.
