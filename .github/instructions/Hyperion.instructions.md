---
applyTo: '**'
---
# Hyperion Engine - AI Assistant Instructions

## Project Overview
Hyperion is a modern 3D game engine written in C++ with Vulkan rendering, multi-threading, ECS architecture, and C# scripting support. It targets Windows, macOS, and Linux with features like hardware ray tracing, PBR rendering, and GPU particles.

## Core Architecture Patterns

### Reflection System
The engine uses extensive reflection via macros:
- `HYP_STRUCT()` - Mark structs for reflection
- `HYP_FIELD()` - Mark fields with properties like `Serialize = true, Editor = true`
- `HYP_ENUM()` - Mark enums for reflection
- `HYP_MAKE_ENUM_FLAGS(EnumName)` - Enable bitwise operations on enum classes
- Use `EnumFlags<EnumType>` instead of raw integers for flag fields

Example pattern from `AudioComponent.hpp`:
```cpp
enum class AudioComponentFlags : uint32 {
    NONE = 0x0,
    INIT = 0x1,
};
HYP_MAKE_ENUM_FLAGS(AudioComponentFlags);

HYP_STRUCT(Component, Editor = true)
struct AudioComponent {
    HYP_FIELD(Property = "AudioSource", Serialize = true, Editor = true)
    Handle<AudioSource> audioSource;
    
    HYP_FIELD()
    EnumFlags<AudioComponentFlags> flags = AudioComponentFlags::NONE;
};
```

### Entity Component System
- Entities are lightweight IDs managed by `EntityManager`
- Components are structs marked with `HYP_STRUCT(Component, ...)`
- Systems extend `SystemBase` and process components via `EntityManager`
- Use tags like `EntityTag::UPDATE_RENDER_PROXY` for change notifications

### Handle System & HypObject Architecture
- `Handle<T>` for memory-managed references to HypObjects (engine's object system)
- `WeakHandle<T>` for non-owning references to HypObjects  
- Always check validity with `handle.IsValid()` before use
- HypObjects inherit from `HypObjectBase` and use Handle<T> for management

#### HypObject System Details
The HypObject system is the engine's core object management framework:

**Object Creation & Lifecycle:**
```cpp
// Create objects using CreateObject<T>() - returns Handle<T>
Handle<Node> node = CreateObject<Node>(NAME("MyNode"));

// Initialize objects after creation
InitObject(node);

// Check object state
if (node.IsValid() && node->IsReady()) {
    // Object is ready for use
}
```

**Key Characteristics:**
- All engine objects inherit from `HypObjectBase` 
- Objects are allocated in memory pools (`HypObjectContainer<T>`)
- Automatic reference counting via `Handle<T>`
- Integration with reflection system via `HypClass`
- All HypObjects have a runtime-only unqiue ID (`ObjId<T>`) which stores the integer id as well as the TypeId of the instance class for type safety. Access via `Id()`.
- Support for C# managed objects and scripting
- Objects have initialization states: Uninitialized → Init Called → Ready
- Casting between types using `ObjCast<T>(handle)` - returns `Handle<T>` if valid, else invalid handle
- Fast type checking with `IsA<T>(handle)` to check if handle can be cast to T.
- `SafeDelete(std::move(handle))` to safely destroy objects on the main thread after a few frames (thread-safe). Only necessary if the object may be referenced by other threads, otherwise `Reset()` or simply allowing the handle to go out of scope is sufficient.

**Memory Layout:**
- Each object has a `HypObjectHeader` containing metadata and ref counts
- Object data follows the header with proper alignment
- Reference counting handles object destruction automatically

### NON-HypObject Reference Counting
- `RC<T>` for reference counting types NOT associated with the engine's HypObject system
- `Weak<T>` for non-owning references NOT associated with HypObjects

### Performance & Profiling
- `HYP_SCOPE` - Automatic function profiling (use in every function)
- `HYP_NAMED_SCOPE("label")` - Custom profiling scope
- `HYP_NAMED_SCOPE_FMT("format {}", arg)` - Formatted profiling
- `HYP_DEFER(...)` - Cleanup code that runs at scope exit

### Core Container Library
The engine provides a comprehensive set of optimized containers in the `hyperion::containers` namespace:

#### Primary Containers
- `Array<T>` - Dynamic array with inline storage optimization for small arrays
- `FixedArray<T, Size>` - Compile-time fixed-size array with contiguous memory
- `LinkedList<T>` - Doubly linked list for frequent insertions/deletions
- `StaticArray<T, Size>` - Compile-time array for constexpr operations
- `HashMap<Key, Value>` - Hash table with open addressing and linear probing
- `HashSet<T>` - Hash set for unique elements with fast lookups
- `FlatMap<Key, Value>` - Sorted associative container using flat array storage
- `FlatSet<T>` - Sorted set container using flat array storage  
- `SortedArray<T>` - Auto-sorted array that maintains order on insertion
- `SparsePagedArray<T, PageSize>` - Sparse array with paged memory for large datasets and memory efficiency
- `Stack<T>` - LIFO stack based on Array (inherits from Array<T>)
- `Queue<T>` - FIFO queue based on Array (inherits from Array<T>)

#### Container Features
- **Contiguous Memory**: Most containers use contiguous storage for cache efficiency
- **Inline Storage**: Array<T> uses inline storage for small arrays to avoid heap allocation
- **Span Integration**: Contiguous containers convert to `Span<T>` for safe array view operations

#### Usage Patterns
```cpp
// Dynamic arrays with inline optimization
Array<int> numbers = {1, 2, 3, 4};
numbers.PushBack(5);

// Fixed-size arrays for known sizes
FixedArray<float, 4> matrix_row = {1.0f, 0.0f, 0.0f, 1.0f};

// Flat containers for sorted data
FlatMap<String, Handle<Texture>> textures;
textures.Insert({"grass", grassTexture});

// Stack/Queue operations
Stack<Handle<Node>> nodeStack;
nodeStack.Push(rootNode);
Handle<Node> current = nodeStack.Pop();
```

### Name System
The engine uses a sophisticated string interning system for efficient string operations:

#### Name Types
- `Name` - Interned string stored in global registry with fast O(1) comparisons
- `WeakName` - Hash-only name that doesn't require registry lookup, used for temporary comparisons
- `NAME("string")` - Macro for compile-time name creation from string literals

#### Name Usage Patterns
```cpp
// Create names from compile-time strings
Name objectName = NAME("MyObject");

// Create names from runtime strings
Name dynamicName = CreateNameFromDynamicString("RuntimeName");

// Weak names for temporary usage
WeakName tempName = CreateWeakNameFromDynamicString("TempName");

// Fast comparisons (hash-based)
if (name1 == name2) { /* ... */ }

// String lookup
const char* str = objectName.LookupString();
String stringRep = objectName.ToString();
```

#### Key Characteristics
- **String Interning**: Names store only hash values, strings kept in global registry
- **Fast Comparisons**: O(1) equality/inequality operations via hash comparison
- **Thread-Safe**: Registry operations are properly synchronized
- **Memory Efficient**: Duplicate strings stored only once in registry
- **Unique Generation**: `Name::Unique("prefix")` creates unique names with prefixes

### String System
The engine provides a comprehensive Unicode-aware string system:

#### String Types
- `String` - Default UTF-8 string (alias for `String<UTF8>`)
- `ANSIString` - ANSI/ASCII character strings
- `WideString` - Platform-specific wide character strings
- `UTF16String` - UTF-16 encoded strings
- `UTF32String` - UTF-32 encoded strings

#### String Features
- **Unicode Support**: Native UTF-8, UTF-16, UTF-32 support with automatic conversions
- **Inline Storage**: Small strings avoid heap allocation (64-byte inline buffer)
- **Efficient Operations**: String concatenation, searching, manipulation
- **Memory Management**: Based on Array<T> with automatic memory handling

#### String Usage Patterns
```cpp
// Default UTF-8 strings
String text = "Hello, World!";
String combined = text + " More text";

// Type conversions
WideString wide = text.ToWide();
String utf8 = wide.ToUTF8();

// String operations
String joined = String::Join(container, ',');
Array<String> parts = text.Split(' ');

// Comparison and searching
if (text.Contains("Hello")) { /* ... */ }
SizeType pos = text.Find("World");
```

### UI System Architecture
The UI uses a selective update system to avoid expensive tree traversals:
- `UIUpdateManager` batches updates by type (`UPDATE_SIZE`, `UPDATE_POSITION`, etc.)
- Call `SetDeferredUpdate(UIObjectUpdateType::type, updateChildren)` instead of immediate updates
- Updates are processed in dependency order (size → position → visibility → mesh)

### Threading & Task System
The engine uses a sophisticated multi-threaded task system:

#### Core Threading Concepts
- `TaskSystem` - Central task scheduler with multiple thread pools
- `TaskBatch` - Groups of related tasks that can be executed in parallel
- Thread pools: `THREAD_POOL_GENERIC`, `THREAD_POOL_RENDER`, `THREAD_POOL_BACKGROUND`
- `Threads::AssertOnThread(threadId)` - Validate code runs on expected thread

#### Thread Safety Patterns
```cpp
// Check current thread
Threads::AssertOnThread(g_gameThread, "Must be called from game thread");

// Atomic operations for thread-safe counters
AtomicVar<uint32> counter;
counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

// Thread-safe delegates for callbacks
Delegate<void> onComplete;
```

## Build System

### Development Workflow
- Primary build: `./tools/scripts/BuildHyperion.sh` (Unix) or `build.bat` (Windows)
- CMake generates to `build/` directory
- Build outputs go to `build/bin/`
- Use VS Code task "Build Hyperion" for integrated building

### Platform-Specific Notes
- macOS: Requires MoltenVK for Vulkan→Metal translation
- Set `VULKAN_SDK` environment variable
- iOS builds supported with `--ios` flag

### Submodules
Critical dependencies are Git submodules in `submodules/`. If missing features, run:
```bash
git submodule update --init --recursive
```

## Debugging & Threading

### Thread Safety
- Main thread: Game logic, ECS updates
- Render thread: Vulkan commands, GPU operations
- Use `Threads::AssertOnThread(g_gameThread)` for validation
- UI updates must happen on main thread

## Scene Hierarchy & Node System

### Node Hierarchy
- `Node` is the base class for scene graph objects (inherits from `HypObjectBase`)
- Scene graph uses parent-child relationships: `AddChild()`, `RemoveChild()`, `GetParent()`
- Transform propagation: local transforms combine with parent transforms automatically

### Entity-Component Architecture
- `Entity` inherits from `Node` - entities ARE nodes in the scene graph
- Components attach to entities via `EntityManager`: `GetComponent<T>()`, `AddComponent<T>()`
- Entity tags for change tracking: `AddTag<EntityTag::UPDATE_RENDER_PROXY>()`
- Scene octree for spatial queries and frustum culling

Example Node hierarchy pattern:
```cpp
Handle<Node> parentNode = CreateObject<Node>(NAME("Parent"));
Handle<Entity> childEntity = CreateObject<Entity>(NAME("Child"));
parentNode->AddChild(childEntity);

// Transform propagation happens automatically
parentNode->SetLocalTranslation(Vec3f(1, 0, 0));
```

## Rendering Architecture

### Vulkan Backend
- Pipeline creation: `MakeGraphicsPipeline()`, `MakeComputePipeline()`, `MakeRaytracingPipeline()`
- Shader system: `VulkanShader` with multiple `VulkanShaderModule` (vertex, fragment, compute, etc.)
- Descriptor management: `DescriptorTable` for resource binding
- Command buffer recording: Graphics, compute, and async compute queues

### Material & Mesh System
- Materials define rendering properties: PBR parameters, textures, blend modes
- `MeshComponent` links entities to renderable geometry
- `MaterialAttributeSet` controls pipeline state (culling, depth testing, etc.)
- Instance data for efficient rendering of multiple objects

## HypScript Language

### Language Features
- Custom scripting language with C-like syntax
- Bytecode compilation via `Script_Interpreter` and `Script_Instance`
- Integration with engine's reflection system via `HypClass` interop
- Support for calling engine methods and accessing properties from scripts

### HypScript Workflow
```cpp
// Compile and run HypScript
SourceFile sourceFile("script.hs");
ErrorList errors;
Script_Instance* instance = HypScript::GetInstance().Compile(sourceFile, errors);
if (instance) {
    HypScript::GetInstance().Run(instance);
}
```

### C# Scripting Alternative
- .NET Core integration for C# scripts
- `ScriptComponent` can use either HypScript or C# via `ScriptLanguage` enum
- Managed-native interop through `dotnet::ManagedObject` and reflection bindings

### Delegates & Functional Programming
The engine provides a robust delegate system for callbacks and event handling:

#### Delegate System
- `Delegate<ReturnType, Args...>` - Multi-cast delegate for function callbacks
- `Proc<ReturnType(Args...)>` - Simple function wrapper with inline storage
- `DelegateHandler` - RAII handle for managing delegate subscriptions
- `DelegateHandlerSet` - Container for managing multiple delegate handlers

#### Usage Patterns
```cpp
// Create and use delegates
Delegate<void, int> onValueChanged;

// Subscribe to delegate
DelegateHandler handler = onValueChanged.Bind([](int newValue)
    {
        HYP_LOG(Core, Info, "Value changed to: {}", newValue);
    });

// Invoke delegate
onValueChanged(42);

// Handler automatically unsubscribes when destroyed
```

## Common Patterns
- Always include required headers: If using `HYP_SCOPE` to enable profiling, need to include `#include <core/profiling/ProfileScope.hpp>`
- Enum flags need `#include <core/utilities/EnumFlags.hpp>`
- Deferred updates: Use `SetDeferredUpdate()` instead of immediate UI updates
- Memory management: Use `Handle<T>` for engine objects, `RC<T>` for non-engine ref counting, `UniquePtr<T>` for unique ownership
- Thread safety: Always validate thread context with `Threads::AssertOnThread()` in thread-sensitive code
- Object lifecycle: Check `handle.IsValid()` before dereferencing, use `InitObject()` after `CreateObject()`

## Code style & Conventions
- Refer to existing code style. 4-space indentation, braces on new lines
- Use `camelCase` for variables and functions, `PascalCase` for types and classes
- Limit `auto` usage to iterators and complex types where the type is obvious from context (e.g complex template types)
- Pointer ownership should be clear - prefer `Handle<T>` for engine objects, `RC<T>` for non-engine reference counting, `UniquePtr<T>` for unique ownership, and raw pointers where appropriate
- Standard library usage is minimized; prefer engine containers and utilities over STL equivalents
- Logging is via `HYP_LOG(...)`. You may need to include `#include <core/logging/Logger.hpp>`, but limit including this file to .cpp files. The syntax of the logging macros is as follows:
  - `HYP_LOG(ChannelName, Error, "Message: {}", variable);` where `ChannelName` is a predefined logging channel like `Core`, `Render`, etc. Channels should be defined in `core/logging/LogChannels.hpp`. The log levels are `Debug`, `Info`, `Warning`, `Error`, and `Fatal` (which crashes the engine with the error).
  - For debugging code, you can use the convenience macro `HYP_LOG_DEBUG("message: {}", variable);` which logs at the Debug level.
- Asserts use `Assert(...)` macro which takes a condition and optional formatted message. For debug-only asserts, use `AssertDebug(...)`.
- For code that should never be reached, use `HYP_UNREACHABLE()` which logs an error and crashes the engine.
- To interact with the reflection system, use `TypeId::ForType<T>()` to get the TypeId of a type, and `HypClass::GetClass(TypeId)` to get the `HypClass`. Use `Field` and `HypMethod` to access fields and methods reflectively.

## Code Generation
The `generated` directory contains reflection data. Never edit these files directly - they're regenerated from source annotations.

## Copilot Instructions
Reduce summary of your changes to at most a single paragraph. If the change is trivial (e.g. fixing a typo, updating comments, formatting), simply state that. Otherwise, briefly describe the purpose of the change and any important details.

When suggesting code, ensure it adheres to the project's coding style and conventions. Avoid using `using namespace` directives; prefer fully qualified names or explicit `using` declarations for specific types or functions.