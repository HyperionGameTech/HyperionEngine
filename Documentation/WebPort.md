# Web port plan (WebAssembly + WebGPU, min spec)

Status: plan only, nothing implemented. Written 2026-10-07 from a read-only survey of the tree.
Statements about Tint, Dawn and Emscripten behaviour are from memory and are marked **verify** where the plan depends on them; Milestone 0 exists to check them.

## 1. Scope

**In:** standalone game (`hyperion-sample` / a `Game` subclass) running in Chrome/Edge desktop, single player only for the first pass (both confirmed 2026-10-07).

**Out (by decision):** editor and Avalonia, C# / .NET host, Strata scripting, runtime DXC, plugins, Steam, baking, commandlets on the target.

**Out (forced by WebGPU):** hardware ray tracing (DDGI, RT reflections, path tracer), bindless textures, async compute, wireframe fill mode.

**Deferred to after the first playable build:** multiplayer, streamed asset delivery, GPU-driven indirect rendering, particles, GPU timers, gamepad, compressed textures.

The closest existing configuration is the Android/iOS shipping build: `HYP_SHIPPING` already gives no editor, no .NET host, no Strata JIT, no commandlets, static libraries and precompiled shaders only. The web target is that configuration plus a new render backend and a new platform layer.

## 2. Target architecture

| Area | Choice |
|---|---|
| Toolchain | Emscripten, wasm32, `-pthread`, static link (`HYP_BUILD_STATIC`), whole-archive for engine libs |
| Graphics API | WebGPU through `webgpu.h`; Dawn natively on Windows, emdawnwebgpu in the browser |
| Shaders | HLSL → DXC → SPIR-V → WGSL, offline on the host, shipped in the cooked shader assets. Converter: naga (measured, §10) or Tint (unmeasured) |
| Threads | `PROXY_TO_PTHREAD`: the engine's main, sim and render threads are all workers; the browser main thread only forwards input and DOM calls |
| Presentation | Render thread owns an OffscreenCanvas and the device, and is driven by `requestAnimationFrame` instead of a `while` loop |
| File system | WASMFS; cooked `Cache/`, `Content/`, `Config/` fetched into it before `Hyp_Initialize` |
| Audio | Null adapter first, then Emscripten's OpenAL |
| Network | `--singleplayer` first |
| Browsers | Chrome/Edge desktop first; Firefox and Safari in hardening |

### Why the render thread must be rAF-driven

Two WebGPU rules force this, and they shape the backend:

1. A canvas frame is presented when the worker returns to its event loop. A render thread that never returns never shows a frame.
2. `mapAsync`, `onSubmittedWorkDone` and pipeline-async callbacks only fire when the worker returns to its event loop. Nothing on the render thread may block waiting for the GPU.

`RenderThread::Update()` already exists as a single-frame function (used by `-RenderOnMainThread`), so the change is to the thread body in `Framework/Threads/RenderThread.cpp:340-388`, not to the frame code.

**Fallback topology** if OffscreenCanvas + emdawnwebgpu on a pthread does not work (**verify** in M0): render on the browser main thread using the iOS precedent (`-RenderOnMainThread=true`, `--detached`, `Hyp_MainThreadUpdate()` from rAF). This needs every blocking wait removed from the main thread (`EngineDriver.cpp:372-377`, `:471`, sim/render sync semaphores), so it is the more invasive option.

### Min-spec render tier

Start from `Config/EngineConfig.Android.json` as `EngineConfig.Web.json` (the per-platform overlay in `Core/Config/Config.cpp:130-140` already supports this).

| Feature | First build | Notes |
|---|---|---|
| Deferred GBuffer + clustered lighting | on | fullscreen pixel shaders, CPU-built light lists |
| Non-bindless materials | on | `Material` set with 5 textures, already used on Mac/iOS |
| CSM / spot / point shadows | on | needs `depth-clip-control`; shadow sampling changes in §4 |
| Env probes, sky | on | readbacks become asynchronous (§3) |
| Lightmaps (baked on desktop) | on | |
| SSR, HBAO, bloom, tonemap, TAA, height fog, decals, UI, sprites | on | TAA and bloom extract are compute |
| Clouds | on, after the basics | compute-heavy; two storage formats change (§4) |
| Indirect rendering + HZB occlusion culling | off | uses `InterlockedAdd` into the indirect buffer; Android already runs without it |
| Depth pre-pass, parallel rendering | off | follow from the two above |
| Particles | off | indirect draw + atomics + RW buffer in vertex stage |
| RT, DDGI, SSGI, fog volumes | off | |
| GPU timers, async compute | unsupported | `IsSupported()` returns false |

## 3. Workstream A: WebGPU backend

Size reference: Vulkan is 64 files / 17.0k lines, DX12 is 48 files / 13.8k lines. Expect WebGPU to be smaller (no memory allocator, no barriers, no RT, no fences), roughly 8–10k lines.

### A1. Third-backend seam (do on desktop first)

- Add `HYP_WEBGPU` to the ~25 `#if HYP_VULKAN / #elif HYP_DX12` include tails, `RenderTypes.hpp:22-66`, `CommandRecorder.hpp:9-11`, `RenderMemory.hpp`, `EngineGlobals.hpp:139-143`, `HyperionEngine.cpp:157-161`.
- Make `RENDERING_BACKEND` a real CMake option (`Source/CMakeLists.txt:657-680` hard-codes DX12 on Windows).
- Move surface creation out of the window classes: `AppContext.hpp` includes `vulkan_core.h` and exposes `GetVkSurface()`; replace with a backend-neutral native handle that each backend turns into a surface.
- Real leaks to handle: `Texture.cpp` (DX12 row-pitch padding; WebGPU needs 256-byte `bytesPerRow` alignment for buffer↔texture copies, so the same code path generalises), `CommandRecorder.cpp:367,599` (blit and mip generation), `DeletionQueue.hpp`, `ParticlesPass.cpp:46-50`.

### A2. Object mapping

| Engine type | WebGPU | Notes |
|---|---|---|
| `Device`, `RenderInterface` | instance, adapter, device, queue | one queue; request limits listed in A6 |
| `Swapchain` | configured surface | present is implicit at end of rAF callback |
| `GpuBuffer` | `WGPUBuffer` + CPU shadow copy | see A3 |
| `GpuImage`, `GpuImageView`, `Sampler` | texture, view, sampler | no 3-channel formats; no border clamp |
| `Framebuffer`, `Attachment` | render pass descriptor | `Clear` maps to `loadOp`; the 6-layer env-probe framebuffer becomes per-face views |
| `GraphicsPipeline` | family of `WGPURenderPipeline` | see A4 |
| `ComputePipeline` | compute pipeline | `DispatchIndirect` exists in WebGPU |
| `ShaderInstance` | shader module from WGSL text | SPIR-V directly on native Dawn for early bring-up |
| `DescriptorSet` / `DescriptorTable` | bind group / layout | see A5 |
| `CommandBuffer`, transient command buffers | command encoders | submitted in order on the single queue |
| `InsertBarrier`, `ResourceState` | no-op | but see usage-scope audit in A7 |
| `AsyncCompute`, `GpuTimerBackend` | stubs reporting unsupported | |
| `TopLevelAS`, `BottomLevelAS`, `RayTracingPipeline` | stubs | unreachable with `rayTracing = false` |
| `SingleTimeCommands::Execute` | not implementable as blocking | only caller is `Texture::Readback`, which has no callers; assert |

### A3. Buffers, readbacks and frame pacing

- **Writes.** `GpuBufferBase::Map()` returns a persistent pointer that the engine writes every frame (`CBufferAllocator.cpp:169,193`, `IndirectDraw.cpp`, `RawBuffer.cpp:70`). Implement as a CPU shadow copy; `Flush` (or submit) uploads dirty ranges with `wgpuQueueWriteBuffer`.
- **Readbacks.** All real call sites run inside `Frame::OnFrameEnd` and assume a synchronous `Read()` once the slot's fence has signalled (`Texture.cpp:979-1100`, `EnvProbePass.cpp:660-712,876-920`, `ShadowMapCaptureState.cpp:283-334`). In the WebGPU backend:
  - at submit, copy each pending readback into a `MapRead` staging buffer and call `mapAsync`;
  - move that frame's `OnFrameEnd` delegates into a pending list and fire them only when all of the frame's maps have resolved, with the data already in the shadow copy.
  - Frame slots are then reusable immediately, because nothing else in WebGPU needs the fence.
- **`PrepareFrame`** becomes a no-op (Vulkan waits with `UINT64_MAX` at `VulkanRenderInterface.cpp:955-983`).
- **Frame limiter** is off on the render thread; rAF paces it. `NumFramesInFlight = 3` stays.
- **Tab hidden.** rAF stops while the sim thread keeps running against the sim/render handoff. Pause the sim on `visibilitychange`.

### A4. Pipeline state

The engine changes depth test/write/compare, stencil, cull, fill, blend, depth clamp and topology through `CommandRecorder` commands; WebGPU bakes all of them into the pipeline (only stencil reference, viewport, scissor and blend constant are dynamic).

- The backend accumulates these into a state key and resolves `(GraphicsPipeline, state key, attachment formats)` to a cached `WGPURenderPipeline` at draw time.
- First use of a variant compiles synchronously and hitches. Use the shader preload list (`shaderpreload.bin`) to warm pipelines with `createRenderPipelineAsync` behind the loading screen.
- `FillMode::Line` is unsupported: ignore `Rendering.DrawWireframe`. `TriangleFan` is unsupported: convert in `GLTFModelLoader.cpp:1849` (already excluded on Apple).
- Clip space matches D3D (Y up in NDC, top-left framebuffer origin, depth 0..1), so the web shader target follows the `DX12` branches (`Shadows.hlsli:50`, `Tonemap.hlsli:125`) and needs no negative-height viewport.

### A5. Bind groups

- The compiler's flat per-set binding index (`ShaderInputSet::CalculateFlatIndex`) maps one-to-one to `@group/@binding`. `MaxDescriptorSetsBound = 4` equals WebGPU's default `maxBindGroups`.
- `ShaderInputGroup` lacks what a bind group layout needs: per-stage visibility, texture view dimension and sample type, storage texture format and access, sampler kind (filtering / non-filtering / comparison), and `minBindingSize`. Extract these with Tint reflection at offline compile and store them on `ShaderInput` (new serialized fields, web variants only).
- Bind groups are immutable: `DescriptorSet::Update` rebuilds the group when dirty. `DescriptorSetCache` is already keyed by layout and contents, which limits churn.
- Dynamic offsets: observed maximum is 3 uniform and 3 storage per layout (defaults are 8 and 4), but the code allows 16 per set. Assert against the device limit.
- Camera, entity-batch and skeleton buffers are storage buffers with dynamic offsets read in the vertex stage. This is fine in core WebGPU, not in compatibility mode.

### A6. Limits and features to request

| Item | Need | Default |
|---|---|---|
| `maxSampledTexturesPerShaderStage` | ~15 (non-bindless terrain: 5 material + 10 terrain) | 16 |
| `maxStorageBuffersPerShaderStage` | 7 (`ApplyFogVolume`), up to 10 in `DebugVis` permutations | 8 |
| `maxStorageTexturesPerShaderStage` | 2 | 4 |
| `maxColorAttachmentBytesPerSample` | 20 (GBuffer) | 32 |
| Feature `depth-clip-control` | CSM depth clamp, `MAF_DEPTH_CLAMP` | optional |
| Feature `float32-filterable` | only if any R32F/RG32F texture is sampled linearly (audit cloud distance) | optional |
| Feature `texture-formats-tier1` | alternative to the R16F/RG16F storage changes in §4 | optional, newer |

### A7. Audits that can only be done against real validation

1. **Usage scopes.** A texture cannot be an attachment and sampled in the same pass, except read-only depth. Check decals, cloud composite (stencil test while reading depth) and lighting.
2. **Render-thread ownership.** In the browser every WebGPU object belongs to one worker. Any RHI call from another thread (transient command buffers have been used off-thread before) must be marshalled. Dawn native is more permissive, so add an explicit thread assert to every backend entry point from day one.
3. **Blit and mip generation.** The non-Vulkan path is compute and requires storage usage on the destination, which WebGPU does not allow for sRGB, BGRA8 or RGB10A2. Implement both as render passes inside the backend.
4. **Formats.** Expand 3-channel data on upload. GBuffer formats (RGBA16F, RGB10A2, R32 uint, RG16F, D24S8) and D16 shadows are all renderable in core. There are no compressed formats in the engine at all (see §7 risk on download size).
5. **Samplers.** Find users of `ClampToBorder`; WebGPU has none.

## 4. Workstream B: shader pipeline

### B1. New compile target (host tool)

- Add `Web` to `ShaderCompileTargetPlatform` and `WebGPU` to `ShaderCompileTargetBackend` (`ShaderCompiler.hpp:89-110`), to the target-pair list (`ShaderCompiler.cpp:3074-3110`) and to `PrecompileShaders --platform/--api`.
- New properties `BACKEND=WEBGPU`, `TARGET=WEB`. In `Include/Defines.hlsli:150-156`, make `BACKEND_WEBGPU` select the DX12 conventions.
- After `CompileHLSL` (`ShaderCompiler.cpp:3530`), run SPIR-V through Tint and store WGSL text in the existing per-stage blob slot. Link Tint into the host commandlet only.
- Compile only non-bindless variants for this target. Exclude the four RT bundles, `LightmapPathTracerCompute`, and the `RT_GI` / `RT_REFLECTIONS` permutations.
- Validate every generated WGSL module on the host as part of precompile, so no shader error is first seen in a browser.
- `CanCompileShaders()` returns false on web, like Android/iOS (`ShaderCompiler.cpp:2180`). A missing variant then falls back to the `Fallback` bundle or asserts (`ShaderManager.cpp:296-339`), so coverage matters: generate `shaderpreload.bin` by playing the native Dawn build.

Things to settle in M0 (**verify**): which SPIR-V version Tint's reader accepts (the compiler passes `-fspv-target-env=vulkan1.2`); whether its output disables the derivative-uniformity diagnostic (implicit-LOD sampling after `discard` is everywhere); how `-fvk-use-dx-layout` offsets for `float3` members and `float3x4` survive WGSL layout rules. If Tint's SPIR-V reader is too strict, the alternatives are naga, or Slang compiling HLSL to WGSL directly.

### B2. Shader source changes

| Issue | Where | Fix |
|---|---|---|
| Storage texture read and written | `Bloom/BloomExtract.hlsl:79` | read from a sampled input instead |
| RW buffer in a vertex/pixel program | `Particles/Particle.hlsl:32` | declare read-only (only read at `:48`) |
| `CalculateLevelOfDetail` | `GeometryPass.hlsl:333`, `DrawCubemap.hlsl:379`, `DrawShadowMap.hlsl:186` | manual LOD from `ddx/ddy` |
| `QuadReadAcrossX/Y` in compute | `EnvProbe/BlurVisibility.hlsl:45-46` | explicit neighbour loads |
| Storage formats outside WebGPU core | R16F cloud shadow map (`CloudPass.cpp:653`); RG16F env-probe visibility (`BlurVisibility`) | R32F and RG32F on web |
| Format-generic storage targets | `TemporalBlending.hlsl`, `SSR/SSGI.hlsl`, `GenerateMipmap.hlsl`, `BlitCompute.hlsl` | explicit `[[vk::image_format]]` per permutation; the last two are unused once A7.3 is done |
| Linear sampling of depth textures | `Include/Shadows.hlsli:90,106,138,199,327-330,399,414,481` | non-filtering sampler or `Gather`; keep comparison sampling as is |
| GBuffer depth read as float | `DeferredDirect.hlsl:232`, `Decal.hlsl:123` | bind as unfilterable-float with a non-filtering sampler |
| `InterlockedAdd` | `ComputeVisibility.hlsl:186`, `UpdateParticles.hlsl:168` | not in the first tier; later needs atomic-typed buffers |
| Struct layout | 75 three-component members outside `Scene.hlsli` | pad to 16-byte alignment where WGSL rejects the offset; keep C++ mirrors in step |

### B3. Shader hygiene found during the survey (independent of the port)

- `Config/Shaders.hmf:184-189` references `RayTracing/copy_border_texels_*.comp`, which do not exist.
- `Include/PostFXInstance.hlsli:6` is GLSL syntax and is included by `FXAA.hlsl:50`; FXAA has no compiled variant.
- Include casing is mixed (436 `"include/..."` vs 32 `"Include/..."`).
- The on-disk shader cache is entirely DX12 + bindless. Sixteen bundles have no compiled variant, and the SPIR-V + non-bindless combination the web needs is currently unexercised on desktop.

## 5. Workstream C: Emscripten build and platform layer

### C1. Build

- CMake: detect Emscripten as its own platform (`Web`, defines `HYP_WEB` + `HYP_UNIX`) instead of falling into "Linux" (`Source/CMakeLists.txt:22-36`); add a wasm32 arch (`:40-56`); force the shipping-style set (static, no editor, no .NET host).
- Add explicit options. Today editor, .NET and Strata are derived from platform checks: `HYP_EDITOR` at `:512-529`, `HYP_DOTNET_ONLY_FOR_EDITOR` at `:542`, `HYP_STRATA` unconditional at `:584-585`. Building with `HYP_STRATA` undefined is untested.
- The native .NET object model in `Engine/DotNET/*.cpp` always compiles in and stubs the host when `HYP_DOTNET_HOST` is off. Leave it; it is inert portable C++.
- Codegen (`Tools/CodeGen`) runs on the host at configure time, as for Android. New `HYP_CLASS` platform types need a rerun.
- Static registration objects (`CommandLineArgumentRegistration`, `CVar<>`, log channels) need whole-archive linking or they are dropped.
- Link flags: `-pthread -sPROXY_TO_PTHREAD -sOFFSCREENCANVAS_SUPPORT -sWASMFS --use-port=emdawnwebgpu -sPTHREAD_POOL_SIZE=<n> -msimd128 -msse4.1`. The SSE paths in `Core/Math` then compile through Emscripten's emulation headers; the AVX path stays off.

### C2. Compile fixes known in advance

| Item | Where |
|---|---|
| `uint64`/`int64` are `unsigned long`/`long` off Windows, which is 32-bit on wasm32. Changing to `long long` makes `size_t` a third distinct type; expect overload fallout. Do this first, on desktop. | `Core/Types.hpp:23-37` |
| `execinfo.h` backtraces | `Core/Debug/StackDump.cpp:14-20,65-109` |
| `HYP_WAIT_IDLE` falls to inline `asm` | `Core/Defines.hpp:327-337` |
| `sys/sysctl.h`, signal handlers, `atexit`, `std::exit` from handlers | `Core/Debug/Debug.cpp:22`, `HyperionEngine.cpp:227-233,358-371` |
| File offsets through `long` (2 GB cap) | `Core/IO/ByteReader.cpp:130-156` |
| Raw TCP/UDP sockets compiled unconditionally | `Core/Net/Socket.*`, `Engine/Net/NetSocketUDP.*`, `CacheClient.cpp` |
| 79 `static_assert(sizeof(...))` to re-check at 4-byte pointers | Core and Engine |
| V-HACD header included unconditionally | `Engine/Physics/ConvexDecomposition.cpp:17` |

### C3. Platform layer

Model it on Android, which already receives events from another thread and queues them (`Platform/Android/AndroidAppContext.cpp`).

- `WebAppContext` / `WebApplicationWindow` in `Engine/System/Platform/Web/`: implement `CreateSystemWindow`, `PollEvents`, the seven `ApplicationWindow` pure virtuals, and a `WebEvent` member in the `PlatformEvent` union (`Engine/Input/Event.hpp:68-187`).
- html5 callbacks on the browser thread push into a locked queue; the engine main thread (a worker) drains it in `PollEvents`.
- Events: keyboard, mouse buttons and movement (`movementX/Y` under pointer lock), wheel, touch, text input, resize with `devicePixelRatio`, focus and visibility.
- Pointer lock and fullscreen need a user gesture and must be requested on the browser thread; `InputManager` already routes mouse-lock changes to the main thread (`InputManager.cpp:224-235`).
- `PlatformUtils`: executable path is a virtual root (pass `--cachedir` / `--contentdir` like the Android shim), battery returns false, network init is a no-op.
- `ShowMessageBox` is synchronous and modal by contract; implement as a console error plus a non-blocking overlay. The fatal-error hook terminates after it.
- No file dialogs (Android has none either).

### C4. Threads and memory

| Thread | Web |
|---|---|
| Main (input pump, 120 Hz) | worker via `PROXY_TO_PTHREAD` |
| Render | worker, rAF-driven, owns the canvas and device |
| Sim, Vis (shared id) | workers |
| Render worker pool (3) | skipped, as on Android/iOS (`HyperionEngine.cpp:303-308`) |
| Foreground pool | `Threads.NumForegroundWorkers = 1–2` |
| Background pool (≤4 + overseer) | cap at 2 |
| Streaming manager + 1 worker | keep |
| NetRequest, GameClient | not started in single player |
| Jolt job pool (`hardware_concurrency() - 1`) | clamp to 2–4 (`JoltPhysicsAdapter.cpp:588-592`) |

About 12 workers; size the pthread pool to match, because thread creation is immediately followed by a blocking wait.

Memory: pool block sizes sum to roughly 250–300 MiB once touched (Asset 64, Render 64, Net 32, Physics 32, Jolt temp 31, RHI 16, debug drawer 16), plus 5 MiB per thread. Add per-platform block sizes and set a budget (suggest 1.5 GB total) well under the 4 GB wasm32 ceiling.

## 6. Workstream D: content and packaging

- **Cook on the host**, as the Android script does (`Tools/Scripts/PackageBuildAndroid.bat`): `PrecompileShaders --platform=web --api=webgpu`, then `BlobStorageCookCommandlet --out-cache --out-content`. Output is `Cache/toc.bin`, one `<bucket>.bin` per bucket, `shaderprops.bin`, `shaderpreload.bin`, `Content/*.hmf` manifests, `Config/` including `Shaders.hmf`. Add `PackageBuildWeb`.
- **BlobStorage reads.** `BlobStorage.cpp:547,710,905,910` memory-maps whole block files. Emscripten's `mmap` of a file is an allocation plus a full read, so this doubles memory. At runtime the mapping is only a transient read source (`AssetObject.cpp:628-766` copies out of it), so put a ranged-read interface behind it and use plain reads on web.
- **Asset discovery.** `AssetRegistry::LoadAssetDescs` scans directories for `*.hmf` (`AssetRegistry.cpp:1546-1630`, with a TODO to use an index file). Write the index at cook time.
- **Delivery, first version:** fetch the whole package into WASMFS with a progress bar before `Hyp_Initialize`. Simple; RAM cost equals package size. Only viable for a small test level: Project410 cooks to 737 MB (§12).
- **Delivery, later:** leave block files on the server and read blobs by HTTP range request with an OPFS cache. `CacheClient::SyncContent` already has this shape (manifest + blob endpoints, progress, loading screen in `Game.cpp:349-413`) but hand-rolls HTTP over raw TCP; its transport would be replaced with fetch.
- **Runtime writes to disable or redirect:** shader cache files (`ShaderManager.cpp:913,930`), config saves, the cache client. Confirm a pure game never reaches `AssetRegistry::SaveDirtyAssets` or the terrain layer writes.
- **Hosting:** HTTPS with COOP/COEP headers (required for SharedArrayBuffer), compressed transfer for `.wasm` and block files. Ship a small dev server script that sets the headers.

## 7. Milestones

Revised 2026-10-07 after the milestone 0 spikes (sections 10-12). Estimates assume one person who knows the engine; they are rough.

| # | Milestone | Exit criterion | Estimate (working-session hours) |
|---|---|---|---|
| 0 | **Spikes** | **Done.** Shader route viable (§10), worker-thread topology works (§11), cooked content too large for whole-package preload (§12) | done |
| 1 | **Desktop groundwork** on existing backends | **Done** (§13). The sample level runs on Windows **Vulkan** in the min-spec config (bindless, RT, indirect and parallel rendering off) from a cooked package with `CompileOnTheFly` off | measured: 0.8 h |
| 2 | **WebGPU backend on native Dawn**, including the web shader target (B1), the backend seam (A1) and the web-gated shader fixes (B2) | **Done** (§14). Sample level matches the Vulkan min-spec frame using WGSL modules; Dawn validation clean; no stall on the GPU in a 936-frame run apart from one frame-slot wait; readbacks asynchronous; the device arrives through a callback chain | measured: 3.8 h |
| 3 | **Emscripten core build** (can overlap 2), including the `uint64` typedef | **Done** (§15). Engine boots on a worker in Chromium, reads the cooked package through WASMFS, loads `MainWorld`, sim ticks. Ranged BlobStorage reads were not needed for this and moved to milestone 6 | measured: 1.9 h |
| 4 | **Browser bring-up**, including a small test level and reachable-only engine cooking | **In progress** (§16): `MainWorld` renders in Chromium. The frame matches desktop to within frame-to-frame noise, mouse look works, and the engine share of the package is cooked from a recorded list (602 MB to 425 MB). Left: a small test level, package within the size budget | 3–6 h left |
| 5 | **Hardening** | memory budget held, pipeline warm-up, resize/DPI, tab visibility, device loss, audio, Firefox and Safari pass, an audit for the 32-bit bug classes in §15 | 4–8 h |
| 6 | **Content size for a real game** | a Project410-sized game loads: block-compressed textures, streamed delivery, cook-time asset index | 5–10 h |

The estimates were originally in person-weeks (milestone 1 was 3–5 weeks). They are now in hours of working-session wall-clock, because that is what is being measured. There are two data points now. Milestone 1 took 0.8 h. Milestone 2, the largest, was estimated at 20–40 h and took 3.8 h, about a third of it waiting on builds and test runs (each backend switch is a six minute rebuild, each test run 60–90 s). The remaining estimates were cut by roughly a third on that evidence, not by the full factor of five to ten, because milestones 3 and 4 depend on a toolchain and a browser that have not been exercised by this engine at all, and their unknowns are of a different kind from writing a backend against a well-specified API.

Milestone 6 is what a real game needs on top of milestones 1–5, and can start any time because both halves are useful on desktop too.

### Milestone timing

Measured wall-clock time from the first edit to the exit criterion being met, taken from `date`. Estimates above are revised from these as milestones finish.

| # | Started | Finished | Elapsed | Notes |
|---|---|---|---|---|
| 0 | 2026-10-07 (not timed) | 2026-10-07 | under one day, three spikes | not measured precisely |
| 1 | 2026-10-07 19:56:41 -0300 | 2026-10-07 20:45:21 -0300 | 48 min 40 s | roughly half of it engine build time (three full builds, four incremental); scope reduced, see §13 |
| 2 | 2026-10-07 20:48:25 -0300 | 2026-10-08 00:38:26 -0300 | 3 h 50 min | first frame at 2 h 04 min, `MainWorld` validation-clean at 2 h 15 min, then 1 h 35 min on the remaining exit criteria and one shadow bug; about a third of the total was builds and test runs; see §14 |
| 3 | 2026-10-08 00:44:48 -0300 | 2026-10-08 02:36:41 -0300 | 1 h 52 min | compiled and linked at 22 min; the rest was booting it. The exit criterion and the first browser frame landed within minutes of each other, so this row includes the start of milestone 4; see §15 |
| 4 | 2026-10-08 02:36:41 -0300 | | | in progress; first frame is counted under milestone 3; see §16 |

### Milestone 1 contents

| Item | Outcome |
|---|---|
| Render backend selectable at configure time | done: `-DRENDERING_BACKEND=`; `BuildHyperion.bat ... vulkan` or `dx12` builds into `Build/Windows/<type>-<backend>` and `Binaries/Windows/<type>-<backend>` |
| Min-spec switches | done: new `Rendering.BindlessTextures` setting on both backends; the rest already existed |
| Vulkan precompile failures | `DrawCubemap` fixed; dead `RTCopyBorderTexels*` definitions removed. `FXAA` and the two lightmapper bundles still fail (not web tier) |
| Unconditional shader fixes | done: `BloomExtract` no longer reads its output; `BlurVisibility` no longer uses quad reads |
| Cook commandlet | `--project` falls back to the base directory and a bad path is now an error; new `--worlds` filter. Shutdown crash not fixed |
| Startup world | new `--world=<name>` game argument |
| Web-gated shader fixes (`CalculateLevelOfDetail`, `isnan`, `HYP_FLOAT_MAX`, shadow depth sampling, storage formats, `Particle` buffer) | moved to milestone 2: they need the web shader target to compile against |
| Third-backend seam, render-thread assert | moved to milestone 2: verified by adding the backend |
| `uint64` typedef, ranged BlobStorage reads | moved to milestone 3: only compilable and testable under Emscripten |
| Small test level, package budget, reachable-only engine cook | moved to milestone 4: needs a level and a decision (§13) |
| Cook-time asset index | moved to milestone 6: a preloaded package can iterate directories, only streamed delivery needs it |

### Changes the spikes made to later milestones

- **Milestone 2 – shader target:** the web shader target stores one WGSL module per content hash, with variants referencing it (28,340 variants are 1,119 modules). The converter is naga plus an access-mode fix-up pass, or Tint if it is built and compared first; only naga has been measured.
- **Milestone 2 – render init:** `RI.Initialize()` cannot complete inside the thread entry; `g_renderInitSignal` is signalled from the device callback (§11).
- **Milestones 3–4 – pacing:** pace the sim from the render handoff. Timed sleeps on workers are about 15.5 ms on Windows, so the 120 Hz main loop and `FrameLimiter` cannot be relied on (§11).
- **Milestone 4 – target content:** the target is the test level, not Project410.

Order inside milestone 2: device and swapchain clear → buffers, textures, samplers → web shader target, shader modules and bind groups → pipeline state cache → fullscreen passes → GBuffer and lighting → shadows → env probes with async readback → post (TAA, bloom, tonemap) → UI and sprites → clouds and fog.

### Milestone 6 contents

| Item | Work |
|---|---|
| Compressed textures | the engine has no BC/ASTC formats; add BC7 (and BC5 for normals, BC4 for single-channel) to `TextureFormat`, the cook and all three backends; `texture-compression-bc` on web. Textures are about 90% of cooked size |
| Streamed delivery | block files stay on the server; blobs read by HTTP range request with an OPFS cache; `CacheClient` transport replaced with fetch (§6) |

### After the first playable build

| Item | Work |
|---|---|
| Multiplayer | extract a socket interface (`NetClient` holds `NetSocketUDP` by value; API is `Bind/Close/SendTo/RecvFrom/IsValid`); WebTransport datagram implementation in JS glue; a WebTransport-to-UDP relay beside the dedicated server keeps the server unchanged. The reliability layer in `NetChannel.cpp` is datagram-based and carries over. |
| Indirect rendering, particles | atomic-typed indirect buffer, read-only particle buffer in the draw program. `InterlockedAdd` already converts and validates (§10) |
| Gamepad | Gamepad API → `CONTROLLER_*` events (today only Steam Input produces them) |
| GPU timers | `timestamp-query`, pass boundaries only |
| Scripting | Strata would need ahead-of-time compilation to wasm32 objects linked into the build; the non-editor AOT path is an unimplemented TODO today (`Source/CMakeLists.txt:637-639`) |

## 8. Risks

Revised 2026-10-07.

| Risk | Status | Impact | Mitigation |
|---|---|---|---|
| Shader conversion rejects much of DXC's SPIR-V | retired for naga (§10); Tint unmeasured | | naga output needs an access-mode fix-up pass |
| OffscreenCanvas + emdawnwebgpu on a pthread does not work | retired (§11) | | |
| Cooked content too large | confirmed (§12): 737 MB for Project410, 298 MB engine baseline | long load, memory pressure | small test level first; reachable-only engine cook in M1; milestone 6 |
| Render pipelines fail where modules passed | open; the spike validated modules only | shader and backend rework in M2 | bring up one pipeline of each kind early in M2 |
| Hidden off-render-thread RHI calls | open | browser-only crashes | thread assert in M1 |
| Pipeline-variant hitches from baked state | open | stutter | async warm-up from the preload list |
| Non-bindless + SPIR-V path is stale on desktop | confirmed: full Vulkan precompile fails today | M2 starts on a broken baseline | milestone 1 exit criterion |
| Coarse worker sleeps | confirmed (§11) | sim and input tick rate | pace from the render handoff |
| Terrain generation and streaming on one wasm worker | open | slow streaming | measure in M4; prebake cells at cook |
| Main-thread proxying deadlocks (audio, DOM) | open | hangs | keep the browser thread free, no blocking waits on proxied results |

## 9. Unrelated defects noticed during the survey

- `Engine/HyperionEngine.cpp:645`: `CoreAPi::GetCommandLineArguments()` in the Android branch looks like a typo for `CoreApi`.
- `Source/PlatformSpecific/Android/app/src/main/**` is tracked but deleted in the working tree.
- `Tools/Scripts/RunCodeGen.sh` checks `./build/hyperion-codegen` but runs `./Build/hyperion-codegen`.
- `Engine/Rendering/CMakeLists.txt:133-159` uses lower-case `vulkan/` and `dx12/`; the directories are `Vulkan/` and `DX12/`.
- `EngineConfig.Android.json` / `.IOS.json` use `EnableDebugDraw`; the cvar is `Rendering.EnableDebugDrawer`.
- `Source/CMakeLists.txt:1259` references a `webgpu_external` target that is defined nowhere.

## 10. Milestone 0(a) result: shader conversion spike (2026-10-07)

Method: `PrecompileShaders.exe --platform=windows --api=vulkan` into a scratch content directory (the engine's own cache was not touched), then every non-bindless, non-RT variant converted SPIR-V → WGSL and validated in Chrome 152 (`createShaderModule`, plus `createComputePipelineAsync` with an auto layout for compute).

**The converter was naga 30.0.1, not Tint.** Chrome validated the output, so pass/fail on the WGSL is authoritative, but Tint's SPIR-V reader was not exercised.

| Stage | Result |
|---|---|
| DXC → SPIR-V | 60,878 Vulkan variants (half bindless), 2.3 GB of blobs. Four bundles fail in DXC: `FXAA`, `LightmapPathTracer`, `LightmapPathTracerCompute`, and 16,384 permutations of `DrawCubemap` |
| Web subset | 28,340 variants, 56,565 stage modules, only **1,119 unique** by content (16 MB SPIR-V) |
| naga → WGSL | 900 of 1,119 converted (12 MB WGSL) |
| Chrome, raw naga output | 298 of 900 valid |
| Chrome, after two mechanical fix-ups | **863 of 900 valid** |

### What failed and why

| Count (unique modules) | Cause | Kind | Fix |
|---|---|---|---|
| 198 | `CalculateLevelOfDetail` (`ImageQueryLod`) in `GeometryPass`, `DrawCubemap`, `DrawShadowMap` pixel shaders | shader | manual LOD, already in B2 |
| 21 | `isnan` in `BloomExtract`, `TAA`, `TemporalBlending`, `ComputeSH`, `SSRSampleGBuffer`, `DeferredLighting.hlsli` | converter gap | replace with `x != x`, or use a converter that supports it |
| 516 | read-only buffers emitted as `read_write` (naga ignores DXC's `NonWritable` member decoration); rejected in vertex stages | converter artifact | fixed in the spike by rewriting from the SPIR-V decorations |
| 50 | storage textures emitted as `read_write` although only written | converter artifact | fixed in the spike by usage scan; only `UpdateProbeData` (DDGI) really reads its target |
| 26 | `HYP_FLOAT_MAX` (`Defines.hlsli:137`) printed as an out-of-range `f32` literal | converter artifact | use a slightly smaller constant |
| 4 | `textureSample` in non-uniform control flow: `BloomBlur`, `BloomUpsample`, `DeferredDirect`, `HBAO` | shader | `diagnostic(off, derivative_uniformity)` or `SampleLevel` |
| 4 | `QuadReadAcross*` in `BlurVisibility` | shader | already in B2 (or require the `subgroups` feature, which this Chrome exposes) |
| 2 | `workgroupBarrier` in non-uniform control flow in `UpdateProbeData` | shader | DDGI only, outside the web tier |
| 1 | `Particle.hlsl` RW buffer in the vertex stage | shader | already in B2 |

### Conclusions

- The HLSL → SPIR-V → WGSL route is viable. Derivative uniformity, struct layout and atomics, which the plan listed as risks, did not show up as broad failures: uniformity hits 4 modules, no layout errors were reported, and `InterlockedAdd` in `ComputeVisibility` and `UpdateParticles` converted and validated.
- naga needs a small post-process step (buffer and storage-texture access modes). Tint may not; that is still unmeasured.
- Ship unique modules, not variants. 28,340 web variants collapse to 1,119 modules, so the web package should store WGSL once per content hash and have variants reference it.
- Full precompile currently exits with an error on Vulkan. `DrawCubemap` permutes `WRITE_HIT_MASK` with `MODE_SHADOWS`, where `PSOutput` has no `output_hit_mask` (`DrawCubemap.hlsl:179-211,522`); the path tracer bundles use undeclared `MAX_LIGHTS` / `MAX_ENV_PROBES`.

### Not covered by this spike

- Render pipeline creation: vertex/pixel interface matching, bind group layouts, depth-texture sampler rules and attachment formats were not validated, only module-level WGSL.
- Storage texture formats are whatever DXC inferred (`rgba32float` for `float4`), not what the engine binds; explicit formats are still needed (B2).
- Whether the converted shaders produce correct output.

## 11. Milestone 0(b) result: worker-thread WebGPU triangle (2026-10-07)

A standalone C++ test (not engine code) built with Emscripten 6.0.11 and the emdawnwebgpu port, run in Chrome 152 with cross-origin isolation headers. Flags: `--use-port=emdawnwebgpu -pthread -sPROXY_TO_PTHREAD -sOFFSCREENCANVAS_SUPPORT -sPTHREAD_POOL_SIZE=4`.

The primary topology in section 2 works. No fallback is needed.

| Checked | Result |
|---|---|
| `main()` runs off the browser thread (`PROXY_TO_PTHREAD`) | yes |
| Canvas handed from that thread to a second pthread (`emscripten_pthread_attr_settransferredcanvases`) | works |
| Surface created from the `#canvas` selector inside the render pthread | works |
| Adapter, device, pipeline creation on the render pthread | works; callbacks fire once the thread is in its event loop |
| `emscripten_set_main_loop` on the render pthread (rAF in the worker) presents frames | yes, triangle visible |
| Blocking `sem_timedwait` handoff inside the rAF callback | works, zero timeouts over 1,200 frames |
| `mapAsync` readback of a rendered texture | resolves 2-3 frames after submit, pixel values correct |
| Mouse events delivered to a non-browser thread that never returns to its event loop | works, provided the thread calls `emscripten_current_thread_process_queued_calls()` in its loop |
| Dawn validation errors | none |

### Findings for the port

- **Timed sleeps are coarse.** A 4 ms `emscripten_thread_sleep` on a worker took 15.5 ms on average (Windows timer granularity). Engine loops paced by `ThreadSleep` or `sleep_until` will tick at about 64 Hz at best: the 120 Hz main-thread loop (`MainThread.cpp:45`), `FrameLimiter`, the 10 ms `GameClient` poll. Pace the sim from the render handoff rather than from sleeps.
- **Threads that receive proxied callbacks must drain their queue.** The engine main thread's `PollEvents` is the natural place.
- **The render thread body must end in `emscripten_set_main_loop`,** and adapter/device requests only complete after that, so `RI.Initialize()` cannot finish synchronously inside the thread entry as it does today (`RenderThread.cpp:340-388`). `g_renderInitSignal` has to be signalled from the device callback.
- Binary size of the test: 75 KB wasm, 186 KB JS glue.

### Not covered

- Only a 640x480 fixed-size canvas; no resize, device-pixel-ratio or device-loss handling.
- Readback latency was measured with an idle GPU.
- Chrome only.

## 12. Milestone 0(c) result: cooked size (2026-10-07)

`BlobStorageCookCommandlet.exe` (Release build of 2026-09-28) run into scratch output folders; the repo was not modified. gzip -6 stands in for transfer compression.

| Cook | Cache | Manifests (`Content/`) | Total | Cache gzip |
|---|---|---|---|---|
| `--engine-only` | 298 MB | 26 MB, 4,574 files | 324 MB | about 170 MB |
| `Projects/Project410` (327 reachable project assets) | 709 MB | 28 MB, 4,901 files | 737 MB | about 360 MB |

Per-bucket block files:

| File | Engine only | Project410 | gzip ratio |
|---|---|---|---|
| `Textures.bin` | 255 MB | 661 MB | 0.49-0.56 |
| `Shaders.bin` (DX12, bindless; not what web would ship) | 44 MB | 44 MB | 0.49 |
| `Meshes.bin` | 8 MB | 33 MB | 0.36 |
| `AnimationTracks.bin`, `RawData.bin`, `toc.bin` | 4 MB | 4 MB | |

Manifests compress about 30:1 (18.8 MB to 0.6 MB as one archive).

### Conclusions

- **Too big for the "fetch everything into memory" first version.** 737 MB resident before the engine allocates anything, against the 1.5 GB budget in C4, and about 360 MB to download.
- **Textures are 90% of it,** stored uncompressed. Block-compressed textures (BC7 via `texture-compression-bc`, which this Chrome exposes) move from "later" to a prerequisite for a real game. Expect roughly 4:1 on RGBA8 colour data.
- **The engine baseline is 298 MB regardless of project.** The project cook reports reachable project assets, but the engine-only cook is the same size as the engine share of a project cook, so engine content appears to be cooked wholesale. Cooking only reachable engine assets is the cheapest reduction.
- **4,900 loose manifest files** is a second reason for the cook-time index and a single manifest archive (section 6).
- The 44 MB `Shaders.bin` is the DX12 cache. The web equivalent measured in section 10 is 12 MB of WGSL before compression.
- A first web build should target a deliberately small test level, not Project410.

### Problems hit

- The commandlet **segfaults on shutdown** after logging "Blob storage cook complete" (exit code 139, all runs). `PackageBuildWindows.bat` checks `errorlevel`, so packaging would abort.
- `--project` is resolved relative to the executable directory, not the repo. `Projects/Project410` was not found from `Binaries/Windows/Release`; an absolute path works. `Binaries/Windows/Release/Projects/` holds a separate set of older projects.
- Cooking the top-level `Projects/Project24` crashes in the HMF parser (`ParseAssetPathLiteral`) and writes no cache.
- `shaderpreload.bin` is 24 bytes (header only) when written by the cook.

## 13. Milestone 1 result: desktop groundwork (2026-10-07)

Started 19:56:41, finished 20:45:21 (-0300): 48 min 40 s, roughly half of it engine build time.

### What was proven

| Run | Build | Config | Result |
|---|---|---|---|
| 1 | Windows Vulkan (Clang + Ninja, Release) | defaults | Project410 `MainWorld` renders |
| 2 | same | min spec: `BindlessTextures`, `IndirectRendering`, `DepthPrepass`, `ParallelRendering`, RT, SSGI, `FogVolumes` off; validation layers on | renders; no bindless variant requested; shader preload list (50 entries) captured |
| 3 | same | min spec, `ShaderCompiler.CompileOnTheFly` off | renders from the variants captured in run 2 |
| 4 | same | min spec, compile off, freshly cooked package containing the captured variants | renders; no shader misses, no blob lookup errors |
| 5 | Windows DX12 check build | defaults | renders; confirms the shared edits did not break DX12 |

"Renders" means a window capture was inspected and the log checked for errors; nothing was compared pixel for pixel.

### Findings

- **Ship captured variants, not the full permutation set.** The min-spec level needed 502 Vulkan variants (8 MB in `Shaders.bin`). Full Vulkan precompile is 60,878 variants and 2.3 GB; `DrawCubemap` alone has 65,536. The working pipeline is: play the level on a desktop build to capture `shaderprops.bin` + `shaderpreload.bin`, then cook with `--cachedir` pointing at that capture and `--contentdir` at an engine content directory holding the variants.
- **Vulkan validation errors in the current renderer**, each one relevant to WebGPU, which enforces the same rules:
  - `vkCmdCopyImage` from a depth image still in a read-only depth layout (asserts `srcResourceState == ResourceState::CopySrc`, six times per run);
  - a draw sampling an image whose layout is still undefined;
  - cloud noise storage images declared as `Rgba32f` in SPIR-V but bound as `R8G8B8A8_UNORM` (the format-inference problem in B2);
  - buffers not destroyed before device destruction at shutdown.
- **A non-shipping game build cannot start from a locally cooked package without the cache server:** it prompts unless `<cachedir>/Engine.hmf` exists (`EngineDriver.cpp:512`). The runs above used an empty marker file. A Shipping build avoids this and is the configuration the web build mirrors; it was not built here.
- **Level-scoped assets do not resolve.** `Projects/Project410/Worlds/NewLevel.hmf` references `Game://Scenes/Levels/NewLevel/MainScene`, stored under `Levels/NewLevel/Scenes/`; the cook reports "Failed to resolve asset reference" and no asset code handles a `Levels/` layout. Those levels are not cookable today, so there is no small test level yet.
- **The engine baseline is the size problem for a test level.** A cook with no reachable project content is still 306 MB of cache, 255 MB of it engine textures (terrain layers, grass, the hazmat character).

### Open items

- **Small test level.** Needs a small World that the current tree can cook, made in the editor.
- **Reachable-only engine cook.** Engine assets are also referenced from code, so reachability from project Worlds is not enough. Suggested mechanism: record the engine assets a desktop run actually loads (as the shader preload list does) and cook that list plus whatever the Worlds reference.
- **Cook commandlet crash on shutdown** (exit 139 after a complete cook). No usable debugger on this machine: `cdb` is not installed and the installed `lldb` needs `python311.dll`.
- **`FXAA` bundle** does not compile on any backend (GLSL include).
- One of five game runs did not exit within 20 s of closing the window and was killed.

### Machine notes

- The machine-level `MSBuildSdksPath` points at `C:\Program Files\dotnet\sdk\10.0.201\Sdks`, which no longer exists. The Visual Studio generator build fails on the C# projects because of it, and still fails with it corrected. The Clang + Ninja configuration works and is what was used.
- Each build re-copies `Config/` into the output folder, so the min-spec settings have to be reapplied to `Binaries/Windows/Release-Vulkan/Config/EngineConfig.json` after a build.
- `Build/` did not exist at the start; it now holds `Release-Vulkan` and `Release-DX12` trees (3.2 GB each), with matching 1.1 GB output folders under `Binaries/Windows/`. `Binaries/Windows/Release` was not touched.

## 14. Milestone 2 log: WebGPU backend on native Dawn

Started 2026-10-07 20:48:25 -0300. This section is updated as the milestone proceeds.

### Done so far

- **Dawn.** Cloned to `C:/Users/andre/Dev/dawn` (commit in `External/ThirdParty/Source/dawn/DAWN_VERSION.txt`), built with Clang + Ninja as a single shared library (`C:/Users/andre/Dev/dawn-build.bat`). It needs `-DDAWN_SUPPORTS_CXX_MODULES=OFF` with clang-cl. Headers are in `External/ThirdParty/Source/dawn/`; `webgpu_dawn.lib`, `webgpu_dawn.dll` and `tint.exe` are in `External/ThirdParty/Binaries/Windows/Release/` (git-ignored).
- **Web shader target** in `ShaderCompiler`: platform `Web`, backend `WebGPU` (`BACKEND=WEBGPU`, `TARGET=WEB` or `WINDOWS` for native Dawn). Both are opt-in and not part of `AllPlatforms` / `AllBackends`. `PrecompileShaders --platform=windows --api=webgpu` works from any desktop build.
  - DXC emits SPIR-V 1.3 (`-fspv-target-env=vulkan1.1`) for this target; Tint rejects anything newer.
  - `tint.exe` converts each stage to WGSL (`ShaderCompiler.TintPath`, default beside the executable or in the third-party folder) and **the shader blob stores WGSL text**. Bindless variants and ray tracing stages are skipped for this target.
- **Shader fixes for Tint**, all behind `BACKEND_WEBGPU` except the last:
  - `HYP_ISNAN` / `HYP_ISINF` (bit tests on web) replace `isnan` / `isinf` in six shaders;
  - `HYP_TEXTURE_LOD` replaces `CalculateLevelOfDetail` in `GeometryPass`, `DrawCubemap`, `DrawShadowMap`;
  - the `switch` in `DeferredDirect.hlsl` with adjacent case labels is now an `if` (Tint rejects switch fallthrough).

- **Third-backend seam** (checked at 21:34, 46 minutes in): `HYP_WEBGPU` branches in `RenderTypes.hpp`, the 22 backend include tails, the RHI pool, the `RI` global, `CommandRecorder` (blit and mip generation go to the backend, as on Vulkan), `Core/Defines.hpp` (no bindless, no ray tracing) and `Texture.cpp` (the padded copy layout is now shared by DX12 and WebGPU under `HYP_PADDED_TEXTURE_COPIES`). CMake accepts `RENDERING_BACKEND=WebGPU`, `BuildHyperion.bat ... webgpu` selects it, and `Tools/CodeGen` (version bumped to 0.12) guards classes under `rendering/webgpu` with `HYP_WEBGPU`. The Vulkan and DX12 configurations were rebuilt clean after these changes. The `webgpu` configuration does not build yet because the backend classes do not exist.

- **Checkout move** (21:42, 54 minutes in): the work moved from `~/hyperion-engine` (the DominiumGame fork) to `~/Dev/HyperionEngine`, branch `experiment-web`. Everything above was carried over as working-tree edits. Differences in this checkout:
  - backend selection already existed here as `-DHYP_RENDERING_BACKEND=`, so the milestone 1 CMake and build-script change was dropped in favour of it; `BuildHyperion.bat ... webgpu` passes `WebGPU`. There are no per-backend build or output folders (decided 2026-10-07: too many permutations), so switching backend reconfigures `Build/Windows/Release` and replaces `Binaries/Windows/Release`;
  - `--startupworld` already existed, so the `--world` argument from milestone 1 was dropped;
  - the cook commandlet has an extra engine-content output parameter; `--worlds` was re-applied around it;
  - `Deferred/FogVolumeTemporal.hlsl` is new here and also needed `HYP_ISNAN`.
  - **Nothing has been built or run in this checkout.** Every result in §10 to §14 was measured in the other one.

- **Backend classes written** (22:20, 1 h 32 min in): 38 files, about 8,600 lines, in `Source/Engine/Rendering/WebGPU/`. Every type the shared renderer names has a WebGPU class: render interface, command buffer, frame, buffer, image, image view, sampler, attachment, framebuffer, swapchain, shader instance (with the WGSL reflection parser), graphics and compute pipelines, descriptor set and table, texture view cache, async compute, and inert stand-ins for the GPU timer, ray tracing pipeline and acceleration structures.
  - The shared edits were verified first by rebuilding the existing Vulkan configuration in this checkout (passed at 22:06).
  - A class with `HYP_CLASS` but no `.cpp` beside its header makes CodeGen emit an unguarded `<Name>.generated.cpp` into `Source/Generated`, which then breaks every other backend's build. Each backend header therefore needs its `.cpp`, and a stale generated file has to be deleted by hand.

- **First frames** (all on native Dawn, `hyperion-sample.exe` with the milestone 1 cooked package and the min-spec config):
  - 22:28 the backend library compiled on the first attempt after three include fixes; 22:33 the whole engine linked against it.
  - 22:52 first frame: the UI pass (text, buttons, blending) with no validation errors.
  - **23:03 `MainWorld` renders** (2 h 15 min in): geometry, textures, deferred lighting, shadows, clouds, bloom, TAA and tonemapping, visually matching the milestone 1 Vulkan capture, with **zero Dawn validation errors** over a 75 s run. Shaders were compiled on the fly through DXC and Tint.
- **What it took to get from the first frame to the level** (each is a WebGPU rule the engine did not already satisfy):
  - *Dynamic offsets must be multiples of 256.* The engine binds one element of a structured buffer as `element index * element size` (entity instance batches are 2,112 bytes). Structured buffers are GPU-only, so the backend copies the element into a 256-aligned slot of a shared 32 MB buffer and binds that. A copy cannot be encoded inside a pass, so the command buffer is now a list of encoder segments: each pass gets a copy encoder that runs just before it.
  - *Storage texture formats must match exactly.* DXC writes `rgba32float` for every untyped `RWTexture`. The backend rewrites the declaration in the WGSL for the format of the image actually bound, and uses `write` access unless the shader calls `textureLoad` on it. No shader annotations were needed.
  - *Depth textures can only be copied whole.* The shadow atlas clears a region by copying from a cleared texture; that is now a draw that writes `frag_depth`.
  - *A single aspect of a depth-stencil texture has its own view format.*
  - *Dawn loads `d3dcompiler_47.dll` and `vulkan-1.dll` only from beside its own DLL*; both are copied to the output folder by CMake.
- **Dawn's D3D12 backend cannot be used yet.** It compiles through FXC unless Dawn is built with `DAWN_USE_BUILT_DXC`, and FXC fails on `GeometryPass` (`E_FAIL`). Chrome uses DXC, so this is a property of this Dawn build, not of the port. Native runs use Dawn's Vulkan backend (`Rendering.WebGPU.Backend`, default `vulkan`) and are held to the 256-byte offset alignment that D3D12 browsers report.

- **Exit criteria** (finished 2026-10-08 00:38:26, 3 h 50 min):
  - *No stall on the GPU.* A frame slot is not recorded into again until the GPU work and the readbacks of the frame that last used it have completed, which is also what makes the engine's `OnFrameEnd` readback handlers safe to run. That wait is the only place the backend can stall, and is where the web build will skip a tick instead of spinning. The backend counts its stalls and logs them at shutdown: in the final run, 1 of 936 frames waited for its slot, with 0 blocking readbacks and 0 blocking submits.
  - *Init from a callback.* The adapter and device arrive through `AllowProcessEvents` callbacks; `Initialize()` starts the chain and, natively, pumps events until the device is there. The engine-side signal (`g_renderInitSignal`) is still raised by the caller, which is milestone 3 work.
  - *Matches Vulkan.* Compared against a Vulkan build of this same checkout, same package, same config, captured at the same time into the run: region means agree to within 0.1 (of 255), mean absolute difference 0.8 / 0.5 / 0.6, and 0.15% of pixels differ by more than 24, against 0.08% for two Vulkan runs. The engine's DX12 backend was captured as a third opinion and agrees with both.
- **The one rendering bug found by that comparison:** sun shadows leaked on about 3% of the frame. The backend clamped the viewport to the extent the framebuffer was *described* with, but a shadow cascade renders into part of a larger atlas image at an offset, so every cascade not at the atlas origin was clipped away. The clamp now uses the size of the attachment image. Finding it took about an hour, most of it ruling out other causes (clouds, lightmaps, cascade time slicing, the static shadow cache, alpha cutouts, culling, depth clamping, skipped draws, point lights) by experiment.
- **`Shadows.hlsli`** chose the layout of a literal bias matrix with `#ifdef VULKAN`; WebGPU goes through SPIR-V as well, so it now takes the same branch. This is the only backend-conditional matrix in the shaders (the other is the tonemap matrices, which already default to the SPIR-V layout).

### Known gaps in the backend as written

| Gap | Effect | Planned handling |
|---|---|---|
| Framebuffers with `numLayers > 1` | Only the first layer is rendered; WebGPU has no layered rendering | Not used by the min-spec passes: cubemaps are rendered one face per framebuffer |
| Partial-rect clear of a colour attachment | Logged once and skipped; depth rect clears work (far-plane draw) | Add a colour clear-quad pipeline if a pass needs it |
| Storage textures that are read and written | Formats are now taken from the bound image (see above), but `read_write` access is only allowed on `r32float` / `r32uint` / `r32sint` without `texture-formats-tier2` | Measure which shaders need it in the browser |
| Resource arrays (`count > 1`) | Only element 0 is bound; WGSL has no binding arrays | None in the web tier so far |
| Filtering sampler on an unfilterable texture (depth, `R32F` without `float32-filterable`) | Pipeline creation fails validation | Fix per shader as they come up |
| An attachment sampled in the pass that renders to it | Validation error | Per pass |
| `Map()` on a readback buffer outside a frame, `SingleTimeCommands`, shutdown | Still block; counted, and zero in the game path | Only editor and baking paths use them |
| Device limits | The device requests every adapter limit (dynamic buffer counts, offset alignments) | The web build has to fit the default limits; measure in milestone 3 |
| Pipeline creation | Synchronous, at first draw | Async creation later if hitches matter |

### Tint measurement (replaces the naga numbers in §10)

50 of the 52 bundles with a manageable permutation count convert, 1,760 variants in total. The two that fail are outside the web tier: `Particle` (read-write buffer in the vertex stage) and `UpdateProbeData` (DDGI). `GeometryPass` and `DrawCubemap` were left out of the batch run because of their permutation counts and are exercised at runtime instead.

Tint needs no fix-up pass: read-only buffers come out as `var<storage, read>`, variable names are kept, and `@group` / `@binding` equal the engine's set index and flat binding index.

### Backend design decisions

| Topic | Decision |
|---|---|
| Pipeline state | Baked at creation, exactly like DX12 (`CanDynamicallySetDepthState()` false). No draw-time state variants are needed; A4's concern was already solved by the DX12 precedent |
| Command buffer | A state tracker over one `WGPUCommandEncoder`. Pipeline binds, descriptor set binds, vertex/index binds and viewport are recorded and applied at draw or dispatch time, because the engine binds a pipeline before `BeginCapture` |
| Passes | `Framebuffer::BeginCapture` marks a pending render pass that is begun at the first draw, so a full-target `Clear` before it becomes `loadOp = clear`. Compute pipelines open a compute pass on demand; any copy or barrier closes the open pass |
| Bind group layouts | Derived at bind time from two sources: the WGSL declarations of the pipeline's shader (dimension, sample type, depth, storage format and access, sampler kind, stage visibility) and the resources actually bound (filterable or not, filtering sampler or not). A descriptor set keeps a small cache of bind groups per layout |
| Why not layouts from the shader alone | `ShaderInputGroup` is merged across stages and shared between shaders by `DescriptorSetCache`, and whether a float texture is filterable depends on the bound format (depth, `R32F`), which the shader cannot know |
| Pipeline variants | A `WGPURenderPipeline` / `WGPUComputePipeline` is created per distinct tuple of bind group layouts, resolved at draw time. Normally one per engine pipeline |
| Reflection | A small parser over the WGSL text of each stage at shader-instance creation. The same code runs natively and in the browser because both consume WGSL |
| Buffers | CPU-accessible types keep a shadow copy; `Map()` returns it and marks it dirty, `Flush(offset, count)` marks ranges, uploads happen with `wgpuQueueWriteBuffer` before each submit |
| Barriers | No-ops that only update the engine's tracked state |
| Blit, mip generation | Implemented inside the backend with render passes (as Vulkan does natively), not the compute path, which needs storage usage on the destination |
| Reflection classes | `Tools/CodeGen` needs a `rendering/webgpu` → `HYP_WEBGPU` path entry so the generated class declarations are guarded |

### Left over from this milestone

None of these are part of the exit criterion; they are recorded so they are not rediscovered.

- **Glimmer** (the GI system new in this checkout, on by default, off in the min-spec config) does not run: Tint rejects `GlimmerSWRTProbeAlloc` ("`workgroupBarrier` must only be called from uniform control flow"), and the failed compile then trips an assertion instead of disabling the pass.
- **Dawn's D3D12 backend** needs a Dawn build with `DAWN_USE_BUILT_DXC`. Not needed for the browser.
- **The game exits with an access violation** when the window is closed, after an `EntityManager` assertion in scene shutdown. It happens on Vulkan and DX12 in this checkout as well, so it is not from the port.
- **Vulkan logs `Assert(srcResourceState == ResourceState::CopySrc)`** from `VulkanGpuImage::CopyFrom` a few times per run in the min-spec config (the point-light shadow atlas clear). Not from the port either, and the frame is unaffected.
- **Particles, fog volumes and SSGI** were off throughout and have not been run on this backend.
- **Pipeline creation is synchronous** at first draw, and shaders were compiled on the fly, so the first seconds of every run hitch.
- `PrecompileShaders --contentdir=` is not honoured in this checkout: a run with `--api=webgpu --filter=DeferredDirect` wrote its variants into the git-ignored `Content/Engine/Shaders` cache.

### Notes

- `Config/Shaders.hmf` is read from the output folder's `Config/`, copied at build time.
- A virus scanner briefly locks freshly written scratch files; the Tint step retries when its input cannot be opened.
- Transient command buffers are recorded off the render thread on DX12 (`DX12RenderInterface.cpp`, "usable from any thread"). Native Dawn tolerates this; the browser will not.

## 15. Milestone 3 log: Emscripten core build

Started 2026-10-08 00:44:48 -0300. This section is updated as the milestone proceeds.

### Done so far

- **Toolchain.** Emscripten from the milestone 0 `emsdk`, driven through `emcmake cmake -G Ninja` into `Build/Web/Release`, output in `Binaries/Web/Release`. `Web` is a platform like `Android`, not a backend permutation.
- **`hyperion-core` compiles for wasm32** (10 minutes in). 85 of its 88 files compiled untouched once `uint64` / `int64` became `long long` on Web (`Core/Types.hpp`); the predicted overload fallout from `size_t` being a third type did not happen. The other three needed: no `execinfo.h` (`StackDump.cpp`), and CodeGen's `HYP_STRUCT(Size = N)` assertions, which bake the 64-bit size and now only apply where pointers are 8 bytes (CodeGen 0.13).
- **The whole engine and `hyperion-sample` compile and link** (22 minutes in): `hyperion-sample.wasm`, 57 MB unoptimised-for-size, with Jolt, OpenAL and zlib. What it took:
  - CMake: `Web` platform detection has to look at the toolchain file name, because `EMSCRIPTEN` is not set until `project()`; Web forces the shipping-style static build; flags `-pthread -msimd128 -msse4.1`; backend fixed to WebGPU through `--use-port=emdawnwebgpu` (C++ flags only, the port refuses to link from C, which breaks CMake's C feature checks); zlib's shared library off.
  - Dawn's `VERSION` file was renamed `DAWN_VERSION.txt`: on a case-insensitive filesystem it shadows the C++ `<version>` header for anything with that folder on its include path.
  - Engine sources: five fixes, none of them deep. `Game::IsManagedGame` and `ComponentInterface.cpp` assumed the .NET bindings exist; `InternShaderProperty` was inline but declared `extern` elsewhere; the cache server commandlet is compiled out on Web; one `uint64` to `size_t` narrowing; `pthread_setname_np` replaced by `emscripten_set_thread_name`; `WGPUFeatureName_ImplicitDeviceSynchronization` is Dawn-only.
  - New `Engine/System/Platform/Web/` with `PlatformUtils.cpp` and `ShowMessageBox.cpp`.
- The items section 5 expected to be problems and were not: inline `asm` in `HYP_WAIT_IDLE`, raw sockets, signal handlers, the V-HACD include, and the SSE paths in `Core/Math` all compiled as they are.

### Booting it

Finished 2026-10-08 02:36:41 -0300, 1 h 52 min after the start.

- **Link flags** (`Sample/DefaultGame/CMakeLists.txt`): `PROXY_TO_PTHREAD`, WASMFS, memory growth to 4 GB, a 24-thread pool, and for the browser `JSPI` and `OFFSCREENCANVAS_SUPPORT`. `HYP_WEB_PACKAGE_DIR` names a host folder whose `Config/` and `Content/` are preloaded; `HYP_WEB_NODE_PACKAGE_DIR` instead mounts a folder through the node backend for test runs without a browser.
- **Package delivery.** `Config/` and the manifests in `Content/` are preloaded (4 MB). `Cache/` is served beside the page and read through the WASMFS fetch backend: `Cache/index.txt` lists the block files, each is created as a fetch file under `/package` and symlinked into `/hyperion/Cache`, which stays an ordinary directory the engine can write to. `Tools/Scripts/ServeWeb.py` serves a build with the isolation headers and range support.
- **Web app context** (`WebAppContext`, `WebApplicationWindow`): a window that is the page's canvas. No input yet.
- **Under node** the engine initialises and reads the package; it stops at the renderer, as expected.

### Bugs this found in shared code

Each of these is wrong on any 32-bit target, or on any toolchain with a different static initialisation order, and was only hidden by the desktop builds.

| Bug | Effect on wasm32 | Fix |
|---|---|---|
| `JSON::Undefined()` and its siblings returned file-scope statics that other translation units read from their own static initialisers | a missing command line argument read as an empty string: `--exec` was truthy, and `cacheserver` looked set, so the game waited on a content sync that never ended | function-local statics |
| `String::Size()` on a not-yet-constructed string underflowed | same family as above | returns 0 |
| `BitField::Set` kept its 64-bit word mask in a `size_t` | bits 32 to 63 of every word were never set, and clearing a bit wiped them: the 33rd entity in a `SparsePagedArray` page did not exist | mask is `WordType` |
| indices held in `size_t` compared against `Bitset::NotFound`, a 64-bit all-ones value (`AtlasPacker`, `ResourceTracker`) | never equal, so "not found" was treated as index 0xFFFFFFFF | `NotFound` is now a type that compares equal to the all-ones value of whatever integer it is compared with |
| C# bindings were generated for the Web app context classes | desktop C# build broke | no `HYP_METHOD` on them |

More of the same kind probably remain where nothing in `MainWorld` exercises them. Milestone 5 has an audit for them.

### Not done

- Ranged BlobStorage reads. Each block file is still mapped whole, which on wasm means read whole: 594 MB of textures for `MainWorld`. It works within the 4 GB limit; it belongs with streamed delivery in milestone 6.
- A node-based smoke test in the build.

## 16. Milestone 4 log: browser bring-up

Started 2026-10-08 02:36:41 -0300 (the first frame came out of milestone 3's last half hour).

### First frame

`MainWorld` renders in Chromium 152 (the desktop app's built-in browser) from `Binaries/Web/Release`, served by `ServeWeb.py`, with sun shadows, at the canvas's 1280x720. No WebGPU validation errors were logged.

How the frame gets there:

- **Shader variants** are captured, not built. The WebGPU backend now always uses `TARGET=WEB`, on native Dawn too, so a native Dawn run of the level records exactly the WGSL variants a browser needs. Those 246 variants (4.9 MB) are staged into an engine content folder holding no other backend's variants, and `BlobStorageCookCommandlet --basedir=<staging>` cooks from it. `Shaders.bin` is 3.4 MB; the package is 631 MB of cache, 594 MB of that textures.
- **One thread renders, and it is the thread running `main()`** (`--RenderOnMainThread=true`), which under `PROXY_TO_PTHREAD` is a worker and owns the transferred canvas. JSPI lets it wait on the browser where it has to: the adapter and device requests, readbacks, and a yield per frame (`requestAnimationFrame`, raced with a 100 ms timer because a hidden page gets no animation frames).
- **WebGPU calls from other threads are forwarded.** A WebGPU object in a browser exists only on the thread that made the device, while the engine uploads meshes and textures from the sim and task threads. `WebGPUThreadProxy.hpp` wraps every `wgpu*` function the backend uses: off the device thread, the call is run on the device thread and waited for. The device thread runs these calls from its event loop, once a frame, and from inside any blocking wait: `emscripten_futex_wait` is wrapped at link time (`--wrap`), so a device thread parked on a lock or a signal held by the caller is woken to run the call instead of deadlocking. Native Dawn builds compile none of this.
- Dawn accepts a NaN depth clear value on a pass that loads depth; the browser's bindings reject it. The backend sets one always.

### Second session (2026-10-08, morning)

Asked to track down the "missing shader variants". There were none.

- **"Not linked with DXC" is not a missing variant.** `ShaderCompiler::CanCompileShaders` logs it every time a bundle is loaded. A variant that really is missing logs "Bundle ... does not contain a shader satisfying"; the browser run has no such line. All 246 captured variants cover what `MainWorld` asks for.
- **Keyboard and mouse work.** `WebAppContext.cpp` registers `html5.h` callbacks on the thread that polls events and turns them into the engine's events: keys by `KeyboardEvent.code`, mouse move, buttons, wheel, focus, and pointer lock (requested deferred, since browsers only grant it from a user gesture). Mouse look was checked in the browser; keys and pointer lock were not exercised.
- **Loading went from over a minute to about 25 seconds** by holding one reader on the blob storage for the life of the app (`Platform/Web/main.cpp`). `BlobStorage::Unlock` unmaps every block file when its last reader lets go, and on wasm a mapping is a heap copy of the file, so the 594 MB texture file was being copied in again and again.
- **The canvas can be read from the page** with `createImageBitmap(canvas)` drawn into a 2D canvas, which is how frames were inspected while the pane's own screenshots timed out.

### The missing curtains: a short read behind `mmap`

Reading the lightmap atlas textures back from the GPU showed all four hold exactly the cooked data, so the earlier reading of the swap experiments ("two atlases are empty") was wrong. The curtains were missing for a simpler reason, found by logging vertex bounds at mesh upload: **41 of the level's 103 meshes, every one past a fixed point in `Meshes.bin`, arrived as all zeros**, positions and indices both. The player character's data was in that range too, which is why the camera sat in the wrong place.

WASMFS implements `mmap` as an allocation filled by a single `read`, and if that read comes back short it zero fills the rest and reports success (`system/lib/wasmfs/syscalls.cpp`, `_mmap_js`). `MemoryMappedFile::MapRange` now does the reading itself on Web: allocate, then `pread` in 4 MB pieces until every byte has arrived, and fail loudly if a read stops early. With that, no mesh is empty and the browser frame matches the desktop frame of the same package, curtains and camera included.

This also explains the earlier symptoms that looked like separate bugs: surfaces that seemed unlit were sitting behind geometry that was not there, and the "dark" lightmap pages were lighting the right pixels for meshes that had no triangles.

Why the single read comes back short was not chased further. The fetch backend's own `read` looks correct for both its whole-file and chunked paths, so the truncation is probably in the proxied call between the two.

### Comparison with desktop

The canvas captured in the render worker 60 s after the first frame, against the desktop Vulkan capture of the same package at 60 s: mean absolute difference 0.72 / 0.54 / 0.51 per channel, 0.05% of pixels off by more than 24. Two browser captures 5 s apart differ by more than that (1.02 / 0.60 / 0.71, 0.14%), so the two builds agree to within frame-to-frame noise.

### Input, checked in the browser

Holding W walks the character forward, real key presses arrive with the right codes, the wheel zooms the camera and dragging turns it. Events reach the engine on a worker, too late for it to cancel the browser's own handling, so `index.html` does that: with the canvas focused, keys no longer scroll the page or move focus (Ctrl/Cmd combinations and function keys are left to the browser), the wheel does not scroll, and there is no context menu.

### Reachable-only engine cooking

Engine assets are referenced from code as well as from content, so the cook cannot find what a game needs by walking references. It now takes a list of what a run actually loaded.

- `--record-engine-assets=<file>` on any run appends `Bucket/Name` for every Engine asset as it loads (`AssetRegistry.cpp`). It appends as it goes, so the list survives the crash on exit.
- `Tools/Scripts/FilterEngineAssetList.py` narrows the shader entries to one backend and target. Loading a shader bundle loads every variant of it on disk, so a raw list names all of them: 3,212 of the 3,257 entries recorded for `MainWorld`.
- `BlobStorageCookCommandlet --engine-assets=<file>` cooks only the listed Engine assets, and trims each cooked shader bundle to the variants cooked with it. Without the trim the browser logs about 3,200 unresolved references at start-up.

For `MainWorld` the list comes to 291 assets: 246 shader variants, 30 bundles, ten font atlas textures, the sky sphere's mesh, material and prefab, the font and the blue noise. The cache goes from 602 MB to 425 MB and the manifests from 7.6 MB to 4.4 MB; what is left is the level's own content, 414 MB of it textures. The browser renders the level from this package with a clean log. The staging copy of engine content used for the first package is no longer needed.

The recipe for a web package is now: run the level on a native WebGPU build with `--record-engine-assets`, filter the list for `WEBGPU` / `WEB`, cook with `--engine-assets`.

One thing to know: the list is what one desktop run loaded. An engine asset that only a different code path loads (another level, a feature that was off) will be missing from the package, and the first sign is an unresolved reference in the browser's log.

### Known gaps

- **Intermittent crash in `DeserializeBVHNodeFrom`** while a mesh is paged in, seen about one run in four or five before the storage was kept mapped. Zeroed or partial BVH bytes from the same short read would produce exactly this, so it is probably fixed by the read loop; it has not been seen since, in about fifteen runs, which is not proof.
- **One proxied call at a time.** Each forwarded `wgpu*` call is a round trip to the device thread.
- **Memory.** Each block file is held whole in the heap (594 MB of textures for this level) on top of the fetch backend's own copy.
- The page ticks at 10 Hz while hidden, by design of the timer fallback.
- **Pointer lock is untested.** The desktop app's browser pane refuses it for any page ("The root document of this element is not valid for pointer lock"), so it needs a run in a real Chrome window. Until the lock is granted the mouse still turns the camera, from absolute positions. When the user leaves the lock with Escape, which the browser never delivers as a key, the window hands the game a synthetic Escape so it releases the mouse as on desktop; that path is also untested.
- No resize, no audio check.
- Meshes cooked from the other checkout warn about an unknown `Lod0DataRevision` field. Four entities log an invalid mesh and material; the desktop build logs the same four from the same package.

### Still to do in this milestone

A small test level and the package size budget. Engine content is no longer the size problem; the level's own 414 MB of uncompressed textures is, which is milestone 6's block compression.

