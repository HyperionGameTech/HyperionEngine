# Hyperion Engine


## About

Hyperion Engine is a 3D game engine written in C++, rendering on Vulkan. Currently, it targets Windows, macOS and Linux.

The goal of Hyperion is to be easy to understand and build games with, while still giving you ample control over the game engine.

### Current features include:
* Highly multi-threaded
     * Async task system
     * Async asset loading
     * Parallel command list recording
* Physically based rendering
     * With physically based camera effects (aperture, exposure etc.)
* Post processing
     * HBAO (horizon based ambient occlusion)
     * Screen space global illumination
     * Screen space reflections with glossiness (SSR)
     * Refraction
     * FXAA and TAA for anti-aliasing
     * Tone mapping
* Raytracing - Reflections, Global Illumination (DDGI)
* Procedural, paged terrain generation
* Serialization / deserialization system
* Shader compilation and caching system
     * Integrates with glslang
     * Generates separate shader versions with separate functionalities,
       so you can select one on the fly with the functionality you need, and it will already be compiled and ready to go.
* Skeletal animation
* Scene graph + octree
* GPU occlusion culling
* GPU particle rendering
* Dynamic cubemap rendering with local correction, so reflections match up
* Voxel cone tracing (dynamic global illumination and reflections)
     * Implemented both with a 3D texture and a sparse voxel octree
* Shadow rendering
     * Variance shadow maps
     * Contact-hardened soft shadows
* Scripting engine (Hypscript)


Feel free to contribute anything - We'd love to have some more eyes on this project! Submit an issue if you run into anything, as well.

## Docs (in the works)
[Scripting Language](doc/ScriptingLanguage.md)

## Screenshots
![Particle physics demo](/screenshots/particle_phys2.gif)\
*GPU particles with basic physics simulation*\
![screenshot](/screenshots/ssr.jpg)\
*Screen space reflections*\
![screenshot](/screenshots/point-shadows.jpg)\
*Point light shadows*\
![screenshot](/screenshots/procedural_terrain.jpg)\
*Procedural terrain generation*


## Building

This section will be updated soon. Hyperion uses a pretty simple CMake set up, using vcpkg for MSVC on Windows, and a build script (`build.sh`) for macOS and Linux.

Make sure you install the required packages listed in CMakeLists.txt.

### Known issues
* Currently some resources are not being cleaned up properly on close
* May run into a race condition. This is getting pretty rare though.
* Scripting has issues with generic types
* Binding C++ variables to scripts still not fleshed out
* Voxel cone tracing may have stability issues
