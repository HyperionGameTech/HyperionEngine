# Hyperion Engine


## About

Hyperion Engine is a 3D game engine written in C++, rendering on Vulkan.

The goal of Hyperion is to be easy to understand and build games with, while still giving you ample control over the game engine.

### Current features include:
* Physically based rendering
* Deferred rendering
* Procedural terrain generation
* Voxel Cone Tracing global illumination
* Screen-space reflections
* Parallax corrected dynamic cubemap rendering
* Post processing pipeline
* Contact hardened shadows
* HDR tone mapping
* Skeletal animation
* Scene graph + octree

Feel free to contribute anything - We'd love to have some more eyes on this project! Submit an issue if you run into anything, as well.

## Screenshots

![Apex engine screenshot](/screenshots/screenshot1-ogl.png)
![Apex engine screenshot](/screenshots/screenshot2-vk.png)

## Building

This section will be updated soon. Hyperion uses a pretty simple CMake set up, using vcpkg for MSVC on Windows, and a build script (`build.sh`) for macOS and Linux.

Make sure you install the required packages listed in CMakeLists.txt.
