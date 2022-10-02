# Hyperion Engine


## About

Hyperion Engine is a 3D game engine written in C++, rendering on Vulkan. Currently, it targets Windows, macOS and Linux.

The goal of Hyperion is to be easy to understand and build games with, while still giving you ample control over the game engine.

### Current features include:
* Highly multi-threaded with async task system, async asset loading, etc.
* Physically based rendering
* Procedural, paged terrain generation
* Serialization / deserialization system
* Skeletal animation
* Scene graph + octree
* GPU occlusion culling
* GPU particle rendering
* Post processing system - SSAO, screen space reflections (SSR), screen space refraction, FXAA, Tonemapping
* Dynamic cubemap rendering with local correction, so reflections match up
* Voxel cone tracing (dynamic global illumination and reflections), implemented both with a 3D texture and a sparse voxel octree
* Shadows - Variance shadow maps, contact-hardened shadows
* Scripting engine (Hypscript)
* Ray tracing - DDGI implementation in development

Feel free to contribute anything - We'd love to have some more eyes on this project! Submit an issue if you run into anything, as well.

## Docs
In the works

## Screenshots

Vulkan Renderer:
---

![screenshot](/screenshots/sponza-vk.png)
PBR and image-based lighting
![screenshot](/screenshots/pbr4.jpg)
Screen space reflection:
![screenshot](/screenshots/pbr2.jpg)

OpenGL renderer:
---
![screenshot](/screenshots/screenshot1-ogl.PNG)

## Building

This section will be updated soon. Hyperion uses a pretty simple CMake set up, using vcpkg for MSVC on Windows, and a build script (`build.sh`) for macOS and Linux.

Make sure you install the required packages listed in CMakeLists.txt.

### Known issues
* Currently some resources are not being cleaned up properly on close
* Sometimes, may run into a race condition, some things need to be more thread-safe
* Scripting has issues with generic types
* Binding C++ variables to scripts still not fleshed out
