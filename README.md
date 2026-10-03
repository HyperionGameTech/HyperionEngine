
<div align="center">
  <img width="250" src="Documentation/Images/hyperion_light.png">
</div>
<p align="center">
    <a href="https://hyperionengine.dev">Website</a> • <a href="https://discord.gg/Fv8PwMJEUb">Discord</a>
</p>

<div align="center">
  <img width="700" src="Documentation/Images/terrain-glimmer.jpg">
</div>

## About

Hyperion started as a passion project, back in 2016, and is still worked on daily. Our aim with Hyperion is to offer a high fidelity gaming experience even on low-end hardware using our in-house baking system to prepare as much of the lighting and effects as possible ahead of time.

## Screenshots
| | |
|:---:|:---:|
| ![Hyperion Engine - Baked lightmaps](/Documentation/Images/editor-scene.jpg) | ![Hyperion Engine - Multiplayer, in PIE](/Documentation/Images/multiplayer-editor-1.png) |
| Baked lightmaps and reflections | Multiplayer, in play-in-editor mode |
| ![Hyperion Engine - Glimmer GI](/Documentation/Images/sponza-glimmer.png) | ![Hyperion Engine - Baked lightmaps](/Documentation/Images/sponza-lightmaps.png) |
| Glimmer - realtime GI | Lightmaps - baked offline, static only |

## Some Features
- Clustered deferred shading supporting a large number of dynamic lights while maintaining good frame times. Uses forward clustered shading for translucent materials.
- Visual editor on Windows and macOS, built with Avalonia.
- Offline lightmapper integrated into the editor. Bake lightmaps into the scene,  reflection/irradiance probes for dynamic objects, fog volumes, and other static lighting data such as shadow maps.
- Real time global illumination and reflections via ray tracing on RT capable hardware, with screen-space options for reflections.
- **Glimmer GI** - our (non-hardware RT required!) global illumination system. Probe-based diffuse lighting from software ray tracing against a cached scene representation, with a ground clipmap for terrain and incremental relighting as lights and the sun change.
- Rendering backends: DirectX 12 (the default on Windows) and Vulkan (macOS via MoltenVK, Android, and Linux in future). DirectX 12 is a fully supported, mainline backend.
- Shader compiler system with built in permutations support, and live reload in editor to see changes as you make them.
- Scripting via the [Strata programming language](https://github.com/StrataLanguage/stratac) - JIT compiled, live reload in editor, or AOT linking with shipping builds
- Level streaming via grid-based streaming
- Basic multiplayer setup, with a dedicated server, client-side prediction, replication, etc

## Platforms
Currently, we are focusing our efforts on developing the engine for *Windows*, *macOS*, *Android*, *iOS*, and Steam Deck via Proton. Editor support is available on Windows and macOS. Linux support is planned.

## Getting started
Documentation is a WIP but we have some available on our site: [Documentation](https://hyperionengine.dev/docs)
For any questions you have, please feel free to ask away in [Discord](https://discord.gg/Fv8PwMJEUb).

## AI
AI can a useful tool, but intention is important when building something super complex like a game engine. We aim to keep the usage of AI-generated code limited to bug fixes, UI changes, and occasionally, prototyping / boilerplate. In an effort to maintain transparency, we aim to note where AI is used generally as part of a commit message, comment on the commit on GitHub, or on the PR; although this is on a best effort basis. 

We expect contributors to be honest and upfront about the usage of AI generated code and while AI-generated code isn’t de facto unacceptable, the code must pass the same quality bar that human generated code would. 

## Credits
- [Avalonia](https://github.com/AvaloniaUI/Avalonia)
- [Dock.Avalonia](https://github.com/wieslawsoltes/Dock)
- [Codicons](https://github.com/microsoft/vscode-codicons)
- [Material Icons](https://github.com/google/material-design-icons)
- [meshoptimizer](https://github.com/zeux/meshoptimizer)
- [V-HACD](https://github.com/kmammou/v-hacd)
- [xatlas](https://github.com/jpcy/xatlas)
- [manifold](https://github.com/elalish/manifold)
