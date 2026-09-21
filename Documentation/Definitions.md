# Definitions and Terminology

This document provides definitions and explanations for various terms and concepts used within the engine. It serves as a reference for developers and contributors to understand the terminology used in the codebase, documentation, and discussions.

## I. Core types

### Object
An `Object` is an object that leverages Hyperion's object system, enabling features such as RTTI, reference counting, reflection, implicit serialization. It assigns each object with a unique ID at runtime, which can be used to identify and reference the object throughout the engine.

> Note: The ID of an Object is not persistent across runs, meaning it is only valid for the lifetime of the application.

### `Class` (runtime object)
A `Class` object is an abstraction representing a type in the engine that derives from `ObjectBase`. It contains metadata about the type, such as its name, size, fields, methods, and inheritance hierarchy. `Class` is used for reflection, serialization, scripting, etc.

All types with a `Class` object must derive from `ObjectBase`. To register a class, you need to define it with the `HYP_CLASS()` macro at the top of the class definition. Additionally, the body of the class should include a `HYP_OBJECT_BODY(TheTypeName)` macro invocation to define the class's metadata. To register individual fields and methods, you can use the `HYP_FIELD()` and `HYP_METHOD()` macros respectively.

> Note: After adding a new type that should have a `Class` generated, you must run the build tool to generate the necessary reflection data. This is done by running the `RunCodeGen` script (or just reconfiguring CMake), which will parse the class definitions and generate the required metadata.

### Handle
A [`Handle`](../Source/Core/Reflection/Handle.hpp) is a strong reference to an `Object`. Handles are used for resources like textures, meshes, and other assets that need to be released once they are no longer needed. Also see `WeakHandle` to use a weak reference to an `Object` rather than a strong reference.

To create a new `Handle`, use `MakeHandle<T>()` where `T` is the type of the object you want to create. This will return a `Handle<T>` that can be used to access the object. The object will be automatically destroyed when the last handle to it is released.

## II. Threading
Hyperion splits its work across several dedicated threads, each identified by a global `StaticThreadId` declared in [`Threads.hpp`](../Source/Core/Threading/Threads.hpp). Many objects are owned by a specific thread and will assert when used from a different one. You can use `IsOnThread()` and `AssertOnThread()` to check which thread you are running on, e.g. `AssertOnThread(g_simThread)`.

> Note: The `-RenderOnMainThread` and `-SimulateOnMainThread` command line arguments move rendering or simulation onto the main thread (headless builds always simulate on the main thread). Because of this, always compare against the thread ID globals rather than assuming each one is a separate OS thread.

### Main thread
The main thread (`g_mainThread`) is the thread the application was started on. It polls window and input events, forwarding them to each window's `InputManager`, and executes any tasks that have been enqueued onto its scheduler.

### Sim thread
The simulation thread (`g_simThread`) runs the game logic. Every frame it updates the `Game` and its `World`, including all scenes, `System`s and `Subsystem`s, along with the `AssetManager` and streaming. Most scene management objects, such as a scene's `EntityManager`, are owned by the sim thread.

### Render thread
The render thread (`g_renderThread`) records and submits work to the GPU. It never accesses scene objects directly; instead it reads data written by the sim thread through `RenderProxy` objects (see [Rendering](#v-rendering)).

### Vis thread
The visibility thread (`g_visThread`) keeps the entities in each active view's scenes up to date in the `SceneOctree`, along with their visibility state. By default it is the same thread as the sim thread, but it can be moved onto its own thread with the `-DedicatedVisThread` command line argument.

## III. Scene management oriented:
### Node
A [`Node`](../Source/Engine/Scene/Node.hpp) is a basic building block of the scene graph that has a 3D transform. Nodes can have child nodes, making their transforms relative to their parent recursively.

Nodes can be used to organize entities in a scene, allowing for transformations (translation, rotation, scaling) to be applied hierarchically.

### Entity
An [`Entity`](../Source/Engine/Scene/Entity.hpp) is a special type of `Node` that can have various components attached to it to define its behavior and properties, such as rendering, physics, scripting, etc. Entities can be processed by systems in parallel, depending on the composition of components they have.

### EntityManager
An [`EntityManager`](../Source/Engine/Scene/EntityManager.hpp) owns the entities and components of a single `Scene`, and can be accessed with `Scene::GetEntityManager()`. It is used to create entities (`AddEntity<T>()`), to add, remove and query their components (`AddComponent<T>()`, `RemoveComponent<T>()`, `GetComponent<T>()`, `TryGetComponent<T>()`, `HasComponent<T>()`), and to tag entities with an `EntityTag`.

> Note: Each `EntityManager` has an owner thread (normally the sim thread), and its methods must be called from that thread. During simulation it is locked at various points so that `System`s can safely operate on components concurrently from other threads.

### World
A [`World`](../Source/Engine/Scene/World.hpp) is the top-level container for all scenes in the engine. It manages the lifecycle of scenes and provides a global context for the game. A `World` can have multiple scenes at any given time, each representing a different part of the game world or different levels. Additionally, `World` manages global subsystems such as physics, audio, etc.

### Scene
You can think of a [`Scene`](../Source/Engine/Scene/Scene.hpp) as a region or level in your game's world. It has a root `Node` that can have child `Node`s, which have relative (local) transforms and optionally an `Entity` attached. A `Scene` also has a `SceneOctree` that is used for spatial queries, ray testing, and culling.

### Prefab
A [`Prefab`](../Source/Engine/Scene/Prefab.hpp) is an asset that stores a reusable `Node` hierarchy as a template. Prefabs live in the `Prefabs` [asset bucket](#assetbucket) and are created when importing models (glTF, FBX, OBJ, etc.), or in the editor via "Save as Prefab" on a selection or "New Prefab" in the Content Browser. The root node of a `Prefab` is never part of a live scene; it is kept in a detached scene and only used as the source for new instances.

To create an instance, call `Spawn()`, which clones the root node and returns a new `Handle<Node>` that can be added to a `Scene`. You can look up a prefab by name with `Prefab::Find()` or from the `AssetRegistry`. Each spawned node is tagged with the UUID of the prefab it came from, which can be retrieved with `Prefab::GetSourcePrefabUUID()`.

> Note: Spawned instances are copies, so changes to a prefab's root are not applied to instances that already exist. The editor's "Add to Prefab" action is the exception, as it appends the selected nodes to both the prefab and every live instance of it.

### Swatch
A [`Swatch`](../Source/Engine/Scene/Swatch.hpp) is a state that is applied to the world at runtime that can set different properties for the entities in the Scene. For example, you could have a `Noon` swatch where the sun direction is set to a high angle, and a `Dusk` swatch where the sun has a more grazing angle. Each swatch can have different baked content, enabling you to have a proper baked lighting setup for your different times of day (TOD) and not just relying on realtime lighting.

### Component
A `Component` is data that can be attached to an `Entity`. Components can be used to define the behavior and properties of an entity. For example, a `TransformComponent` can be used to define the position, rotation, and scale of an entity, while a `MeshComponent` can be used to define the mesh, material and skeletal data that will be associated with a given `Entity`.

### System
A [`System`](../Source/Engine/Scene/System.hpp) can process entities in a scene in parallel during simulation, based on the components they have attached. Systems are responsible for updating the state of entities and performing various operations, such as physics simulation, AI, audio, etc.

### View
A [`View`](../Source/Engine/Scene/View.hpp) can be thought of as a slice of a `Scene` that is rendered from a specific camera's perspective. A `View` is used to collect entities and other objects that are visible from the camera's point of view. It contains the camera, the scene(s) to render, and any additional settings for rendering.

Views are the bridge between the scene and the rendering system, allowing for multiple cameras to render different parts of the scene simultaneously. For example, you can have a main `View` for the game's main camera and a separate `View` for shadows.

### Camera
A [`Camera`](../Source/Engine/Scene/Camera/Camera.hpp) is a subclass of `Entity` that provides a viewpoint for rendering the scene. They can have one or many `CameraController`s attached to which process user input and provide camera functionality.

### Light
A [`Light`](../Source/Engine/Scene/Light.hpp) is a subclass of `Entity` that defines a light source in the scene. Just like other types of entities, a `Light` can also be attached to a `Node` in the scene hierarchy, allowing it to inherit transformations from its parent node. Lights can have different types (e.g., directional, point, spot) and properties (e.g., color, intensity) that affect how they illuminate the scene.

### Subsystem
A [`Subsystem`](../Source/Engine/Scene/Subsystem.hpp) is a world-level system that can be added to a `World` to provide additional functionality. Subsystems are not localized to any `Scene` or `View` on the world. Subsystems have an `Update(delta)` method that is called every frame on the sim thread allowing them to perform necessary updates.

### SceneOctree
A [`SceneOctree`](../Source/Engine/Scene/SceneOctree.hpp) is a spatial partitioning structure used to efficiently manage and query the entities in a scene. It divides the 3D space into smaller regions (octants) to optimize collection and collision detection.

## IV. Assets
### AssetObject
An [`AssetObject`](../Source/Engine/Asset/AssetObject.hpp) is the base class for all assets, such as meshes, textures, materials and prefabs. Each asset has a name, a UUID that persists across runs, and an `AssetPath` once it has been registered. Whenever an asset is modified it should be marked dirty with `MarkDirty()`, so that it will be written to disk the next time the registry saves its dirty assets.

The bucket an asset type is stored in is set with the `AssetBucket` attribute of its `HYP_CLASS()` macro, e.g. `HYP_CLASS(AssetBucket = "Prefabs")`.

> Note: Assets flagged as `Transient` are never saved to disk, while assets flagged as `Persistent` are kept loaded in memory.

### AssetBucket
An [`AssetBucket`](../Source/Engine/Asset/AssetBucket.hpp) is a category of asset, such as `Meshes`, `Textures`, `Materials` or `Prefabs`, accessed through the `AssetBuckets` namespace (e.g. `AssetBuckets::Prefabs`). The full list of buckets is defined by `HYP_FOR_EACH_ASSET_BUCKET`, and each bucket maps to a subdirectory of a registry's root path.

### AssetRegistry
An [`AssetRegistry`](../Source/Engine/Asset/AssetRegistry.hpp) keeps track of all the assets under a root directory. There are separate registries for `Game`, `Engine` and `Editor` content. Assets are looked up by bucket and name with `GetAsset<T>()`, and registered with `PutAsset()` (or `PutAssetsDeep()`, which also registers any assets that it references).

Use `GetCurrentAssetRegistry()` to get the registry for the current context, or `GetEngineAssetRegistry()` / `GetEditorAssetRegistry()` for engine and editor content. The current registry can be temporarily overridden for a scope with `GlobalContextScope` and an `AssetRegistryContext`.

### AssetPath
An [`AssetPath`](../Source/Engine/Asset/AssetPath.hpp) identifies an asset by its registry, bucket and name. It is written in the form `Registry://Bucket/Name`, e.g. `Engine://Meshes/InvSphereMesh`.

### Manifest (.hmf)
Each asset is saved to disk as a Hyperion manifest file (`.hmf`), a human-readable text format containing the asset's serialized fields and properties. Manifests are stored at `<registry root>/<bucket>/<name>.hmf`, e.g. [`Content/Engine/Prefabs/InvSphere.hmf`](../Content/Engine/Prefabs/InvSphere.hmf). References to other assets are saved as asset paths (e.g. `@"Engine://Materials/InvSphereMaterial"`) rather than being copied into the manifest.

> Note: Large binary data, such as vertex buffers or texture pixels, is not stored in the manifest itself. It is kept as blob data alongside the manifest, and can be cooked into a `BlobStorage` cache with the `BlobStorageCookCommandlet`.

### AssetManager
The [`AssetManager`](../Source/Engine/Asset/Assets.hpp) imports source files (e.g. `.gltf`, `.fbx`, `.png`) into assets using asset loaders, which are chosen based on the file extension and the requested asset type. For example, `AssetManager::GetInstance()->Load<Prefab>(path)` will load a model file as a `Prefab`. Use `CreateBatch()` to load multiple assets asynchronously.

## V. Rendering
Rendering in Hyperion is kept mostly separate from scene management. Due to the way Hyperion's [multi-threading system](#ii-threading) works, the rendering system is designed to be as independent as possible from the scene management system. As such, some data has to be proxied from the scene management system to the rendering system. This is done via subclasses of `IRenderProxy` which are written to from the sim thread and read from the render thread, buffered over multiple frames to minimize contention.

### RenderProxy
[`RenderProxy`](../Source/Engine/Rendering/RenderProxy.hpp) is a base class for objects that need to be rendered in the scene. It provides a way to pass data from the sim thread to the render thread. Each `RenderProxy` subclass is responsible for providing the necessary data for rendering, such as transform, material, and other properties. The render thread will read these proxies and use them to render the objects in the scene.

### RenderProxyList
[`RenderProxyList`](../Source/Engine/Rendering/RenderProxyList.hpp) is a collection of `RenderProxy` objects that are used to render a specific type of object in the scene. It can track updates on objects (added/removed/changed) between frames via bitwise operations using object IDs.

### RenderGroup
[`RenderGroup`](../Source/Engine/Rendering/RenderGroup.hpp) is a collection of renderable objects, grouped by their rendering attributes (see [`RenderableAttributes.hpp`](../Source/Engine/Rendering/RenderableAttributes.hpp)). `RenderGroup` is used to optimize rendering by batching similar objects together with instancing and performing occlusion culling on them to minimize draw calls. In terms of mapping to the GPU, you can think of a `RenderGroup` as a graphics pipeline.