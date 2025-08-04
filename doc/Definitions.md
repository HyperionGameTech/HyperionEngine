# Definitions and Terminology

(This page is a WIP)

This document provides definitions and explanations for various terms and concepts used within the engine. It serves as a reference for developers and contributors to understand the terminology used in the codebase, documentation, and discussions.

## General Concepts

### HypObject
A `HypObject` is an object that leverages Hyperion's object system, enabling features such as RTTI, reference counting, reflection, implicit serialization. It assigns each object with a unique ID at runtime, which can be used to identify and reference the object throughout the engine.

> Note: The ID of a HypObject is not persistent across runs, meaning it is only valid for the lifetime of the application.

### HypClass
A `HypClass` is a class that is registered with the engine's reflection system. It allows the engine to introspect the class and registered its registered methods and fields.

All HypClasses must derive from `HypObjectBase`. To register a class, you need to define it with the `HYP_CLASS()` macro at the top of the class definition. Additionally, the body of the class should include a `HYP_OBJECT_BODY(TheTypeName)` macro invocation to define the class's metadata. To register individual fields and methods, you can use the `HYP_FIELD()` and `HYP_METHOD()` macros respectively.

> Note: After creating a new `HypClass`, you must run the build tool to generate the necessary reflection data. This is done by running the `RunHypBuildTool` script (or just reconfiguring CMake), which will parse the class definitions and generate the required metadata.

Subclasses of `HypObjectBase` must place a `HYP_CLASS()` macro invocation at the top of the class definition, as well as a `HYP_OBJECT_BODY(TheTypeName)` macro invocation to define the class's metadata.

### Handle
A [`Handle`](../src/core/Handle.hpp) is a strong reference to a `HypObject`. Handles are used for resources like textures, meshes, and other assets that need to be released once they are no longer needed. Also see `WeakHandle` to use a weak reference to a `HypObject` rather than a strong reference.

To create a new `Handle`, use `CreateObject<T>()` where `T` is the type of the object you want to create. This will return a `Handle<T>` that can be used to access the object. The object will be automatically destroyed when the last handle to it is released.

## Scene management oriented:
### World
A [`World`](../src/scene/World.hpp) is the top-level container for all scenes in the engine. It manages the lifecycle of scenes and provides a global context for the game. A `World` can have multiple scenes at any given time, each representing a different part of the game world or different levels. Additionally, `World` manages global subsystems such as physics, audio, etc.

### Scene
You can think of a [`Scene`](../src/scene/Scene.hpp) as a region or level in your game's world. It has a root `Node` that can have child `Node`s, which have relative (local) transforms and optionally an `Entity` attached. A `Scene` also has a `SceneOctree` that is used for spatial queries, ray testing, and culling.

### Node
A [`Node`](../src/scene/Node.hpp) is a basic building block of the scene graph that has a 3D transform. Nodes can have child nodes, making their transforms relative to their parent recursively.

Nodes can be used to organize entities in a scene, allowing for transformations (translation, rotation, scaling) to be applied hierarchically.

Nodes can also have an `Entity` attached to them allowing for the entity to inherit the node's transform. This allows for easy manipulation of entities in the scene graph without having to manage their transforms separately.

### Entity
An [`Entity`](../src/scene/Entity.hpp) is an object in a `Scene` that can have various components attached to it and can be subclassed to provide additional functionality. Entities are used to represent objects in the game world (which may or may not be visible). They can be anything from a 3D model, a light source, a camera, etc. Entities can be added to a `Scene` by attaching them to a node in the scene's hierarchy. Each `Entity` can have multiple components, which define its behavior and properties.

### Component
A `Component` is a modular piece of functionality that can be attached to an `Entity`. Components can be used to define the behavior and properties of an entity. For example, a `TransformComponent` can be used to define the position, rotation, and scale of an entity, while a `MeshComponent` can be used to define the mesh, material and skeletal data that will be associated with a given `Entity`.

### System
A [`System`](../src/scene/System.hpp) is a class that provides functionality to the engine and can be added to a `Scene`. Systems can process entities in a scene in parallel from each other as long as the components they operate on would not conflict with each other allowing for efficient processing of entities.

### View
A [`View`](../src/scene/View.hpp) can be thought of a slice of a `Scene` that is rendered from a specific camera's perspective. A `View` is used to collect entities and other objects that are visible from the camera's point of view. It contains the camera, the scene(s) to render, and any additional settings for rendering. Views are the bridge between the scene and the rendering system, allowing for multiple cameras to render different parts of the scene simultaneously. For example, you can have a main `View` for the game's main camera and a separate `View` for shadows.

### Camera
A [`Camera`](../src/scene/camera/Camera.hpp) is a subclass of `Entity` that provides a viewpoint for rendering the scene. It will also have a `CameraController` attached to it which handles user input and camera movement.

### Light
A [`Light`](../src/scene/Light.hpp) is a subclass of `Entity` that defines a light source in the scene. Just like other types of entities, a `Light` can also be attached to a `Node` in the scene hierarchy, allowing it to inherit transformations from its parent node. Lights can have different types (e.g., directional, point, spot) and properties (e.g., color, intensity) that affect how they illuminate the scene.

### Subsystem
A [`Subsystem`](../src/scene/Subsystem.hpp) is a world-level system that can be added to a `World` to provide additional functionality. Subsystems are not localized to any `Scene` or `View` on the world. Subsystems have an `Update(delta)` method that is called every frame on the game thread allowing them to perform necessary updates.

### SceneOctree
A [`SceneOctree`](../src/scene/SceneOctree.hpp) is a spatial partitioning structure used to efficiently manage and query the entities in a scene. It divides the 3D space into smaller regions (octants) to optimize collection and collision detection.

## Rendering
Rendering in Hyperion is kept mostly separate from scene management. Due to the way Hyperion's multi-threading system works, the rendering system is designed to be as independent as possible from the scene management system. As such, some data has to be proxied from the scene management system to the rendering system. This is done via subclasses of `IRenderProxy` which are written to from the game thread and read from the render thread, buffered over multiple frames to ensure to minimize contention.

### RenderProxy
[`RenderProxy`](../src/rendering/RenderProxy.hpp) is a base class for objects that need to be rendered in the scene. It provides a way to pass data from the game thread to the render thread. Each `RenderProxy` subclass is responsible for providing the necessary data for rendering, such as transform, material, and other properties. The render thread will read these proxies and use them to render the objects in the scene.

### RenderProxyList
[`RenderProxyList`](../src/rendering/RenderProxy.hpp) is a collection of `RenderProxy` objects that are used to render a specific type of object in the scene. It can track updates on objects (added/removed/changed) between frames via bitwise operations using object IDs.

### RenderGroup
[`RenderGroup`](../src/rendering/RenderGroup.hpp) is a collection of renderable objects, grouped by their rendering attributes (see [`RenderableAttributes.hpp`](../src/rendering/RenderableAttributes.hpp)). `RenderGroup` is used to optimize rendering by batching similar objects together with instancing and performing occlusion culling on them to minimize draw calls. In terms of mapping to the GPU, you can think of a `RenderGroup` as a graphics pipeline.