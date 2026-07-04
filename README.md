## About Hyperion

Hyperion Engine is a high performance game engine written in C++20, with a focus on modern rendering techniques such as clustered shading, gpu driven rendering, ray tracing. This project started as a fork of [an earlier engine project](https://github.com/ajmd17/apex-engine) that I started working on in 2016, but has since been almost completely rewritten and redesigned from the ground up. 

![Hyperion Engine Screenshot - Baked lightmaps in Editor view](/Documentation/Images/LightmapBakeEditor.png)

## Platforms
Currently, we are focusing our efforts on developing the engine for *Windows*, *macOS*, *Android*, *iOS*, and Steam Deck via Proton. Editor support is available on Windows and macOS.
> Linux support is planned for the future but not in active development. Contributions welcome on that front if you are interested in that!

To get started, check out the [Compiling the Engine](Documentation/CompilingTheEngine.md) guide to set up your development environment and compile the engine.

## Features
- Real time global illumination and reflections via Ray tracing and screen-space options for non-RT capable hardware.
- Clustered deferred shading supporting a large number of dynamic lights while maintaining good frame times. Uses forward clustered shading for translucent materials.
- Visual editor, supporting Windows and macOS. Supports project files, scene editing, asset importing, etc.
- Offline light baking system integrated into the editor. Bake lightmap volumes into the scene, EnvProbes (reflections and irradiance), particpating medium / fog volumes, and other static lighting data such as shadow maps. (Requries GPU ray tracing support for baking.)
- Integrated real-time path tracer to allow lighting reference before baking.
- Shader compiler system with built in permutations support - just use `PERMUTE(...)` in your shader code to define a permutation set, and the engine will automatically compile all combinations of that permutation and make it available for use at runtime.
- Level streaming and world partitioning system to enable efficient memory usage and larger worlds
- Scripting via C# or our custom scripting language, HypScript.
- Hot reloading for shaders and scripts

## Contributing

If you want to contribute please feel free to submit a pull request! We are open to contributions of all kinds, from bug fixes and documentation improvements to new features and systems.

### Note on AI usage!
Usage of AI tools for contributions should be *explicitly noted*, and should only be used for things like small, non-invasive bugfixes, small QOL fixes and reviewing code before submission. Use your intuition. We won't accept pull requests that have "that vibe-coded smell" - not a quantitative assessment I know, but one knows it when they see it.

## Getting started
[Definitions and Terminology](Documentation/Definitions.md) provides definitions and explanations for various terms and concepts used within the engine. - This is slightly outdated and/or incomplete, but it is a good starting point.

## Console commands and console variables (CVars)

[Console Commands](Documentation/Console.md) are commands that can be executed in the editor's console window to perform various actions or set global states that the engine can use to modify rendering, physics, gameplay, etc.
