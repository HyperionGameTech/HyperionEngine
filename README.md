# Hyperion Engine

Hyperion Engine is a 3D game engine written in C++. Currently, it targets Windows, macOS and Linux and has support for C# scripting via .NET Core.

To get started, check out the [Compiling the Engine](doc/CompilingTheEngine.md) guide to set up your development environment and compile the engine.

## Documentation
[Definitions and Terminology](doc/Definitions.md) provides definitions and explanations for various terms and concepts used within the engine.


---

Dynamic Diffuse Global Illumination (DDGI) | GPU Particles
:-----------------------------------------:|:-------------------------:
![DDG(](/screenshots/ddgi.gif)             | ![GPU particles](/screenshots/particles.gif)

### Some features include:
* Multi threading with task system
* Entity component system
* C# support via .NET Core integration
* Avalonia editor UI (C#)
* PBR and post processing
* Hardware ray tracing support for reflections and global illumination
* Lightmapper with path tracing
* Skeletal animation
* More reflection and global illumination techniques such as
     * Screen space reflections
     * Voxel cone tracing
     * Environment probes
* GPU occlusion culling
* GPU particles