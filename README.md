## About Hyperion

Hyperion Engine is a high performance game engine written in C++20, with a focus on modern rendering techniques such as clustered shading, gpu driven rendering, ray tracing. This project started as a fork of [an earlier engine project](https://github.com/ajmd17/apex-engine) that I started working on in 2016, but has since been almost completely rewritten and redesigned from the ground up. 

![Hyperion Engine Screenshot - Baked lightmaps in Editor view](/Documentation/Images/LightmapBakeEditor.png)

## Some Features
- Clustered deferred shading supporting a large number of dynamic lights while maintaining good frame times. Uses forward clustered shading for translucent materials.
- Visual editor on Windows and macOS, built with Avalonia.
- Offline lightmapper integrated into the editor. Bake lightmaps into the scene,  reflection/irradiance probes for dynamic objects, fog volumes, and other static lighting data such as shadow maps.
- Real time global illumination and reflections via Ray tracing and screen-space options for non-RT capable hardware.
- Shader compiler system with built in permutations support, and live reload in editor to see changes as you make them.
- Scripting via the [Strata programming language](https://github.com/StrataLanguage/stratac) - JIT compiled, live reload in editor, or AOT linking with shipping builds
- Level streaming via grid-based streaming
- Basic multiplayer setup, with a dedicated server, client-side prediction, replication, etc

## Platforms
Currently, we are focusing our efforts on developing the engine for *Windows*, *macOS*, *Android*, *iOS*, and Steam Deck via Proton. Editor support is available on Windows and macOS.
> Linux support is planned for the future but not in active development. Contributions welcome on that front if you are interested in that!

To get started, check out the [Compiling the Engine](Documentation/CompilingTheEngine.md) guide to set up your development environment and compile the engine.

## Contributions

If you want to contribute please feel free to submit a pull request! We are open to contributions of all kinds, from bug fixes and documentation improvements to new features and systems.
> If you submit a PR please indicate where AI-generated code was used (if applicable)

## Getting started
[Definitions and Terminology](Documentation/Definitions.md) provides definitions and explanations for various terms and concepts used within the engine. - This is slightly outdated and/or incomplete, but it is a good starting point.

## Console commands and console variables (CVars)

[Console Commands](Documentation/Console.md) are commands that can be executed in the editor's console window to perform various actions or set global states that the engine can use to modify rendering, physics, gameplay, etc.

## What about AI?
AI can a useful tool when used _in moderation_. But we plan on keeping this codebase mostly written by humans, by hand. Intention is important.

Where _do_ we use AI? Primarily, we aim to keep the usage of AI directed towards:
 - some UI stuff
 - bug fixes `hey, take a look at this callstack...`
 - code review
