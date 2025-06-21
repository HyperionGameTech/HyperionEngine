/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderSkeleton.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/GPUBufferHolderMap.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/lightmapper/RenderLightmapVolume.hpp>

#include <rendering/rt/DDGI.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RenderingAPI.hpp>

#include <scene/Texture.hpp>
#include <scene/View.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Semaphore.hpp>
#include <core/threading/Threads.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/BlueNoise.hpp>

#include <EngineGlobals.hpp>

#include <semaphore>

namespace hyperion {

static constexpr uint32 num_frames = 3; // number of frames in the ring buffer
static constexpr uint32 max_views_per_frame = 16;

// global ring buffer for the game and render threads to write/read data from
static std::atomic_uint g_producer_index { 0 };                             // where the game will write next
static std::atomic_uint g_consumer_index { 0 };                             // what the renderer is *about* to draw
static std::atomic_uint g_frame_counter { 0 };                              // logical frame number
static std::counting_semaphore<num_frames> g_full_semaphore { 0 };          // renderer waits here
static std::counting_semaphore<num_frames> g_free_semaphore { num_frames }; // game waits here when ring is full

struct DrawCallCollectionAllocation;

using DrawCallCollectionPool = MemoryPool<DrawCallCollectionAllocation>;

struct DrawCallCollectionAllocation
{
    uint32 index;
    uint8 alive_frames : 4; // number of frames since this allocation was last used or 0 if it is not used

    ValueStorage<DrawCallCollection> storage;
};

static_assert(std::is_trivial_v<DrawCallCollectionAllocation>, "DrawCallCollectionAllocation must be trivial");

struct ViewData
{
    RenderProxyList render_proxy_list;

    DrawCallCollectionPool pool;                               // pool of draw call collections for this view
    Array<DrawCallCollectionAllocation*> allocation_ptrs;      // for bookkeeping purposes
    Array<DrawCallCollectionAllocation*> prev_allocation_ptrs; // for bookkeeping purposes

    uint32 frames_since_write : 4; // frames_since_write of this view in the current frame, used to determine if the view is still alive if not reset to zero.
};

struct FrameData
{
    LinkedList<ViewData> per_view_data;
    HashMap<View*, ViewData*> views;

    // FixedArray<ViewData*, max_views_per_frame> views;
    // uint32 view_id_counter = 0;

    // used to assign unique view IDs
    Array<DrawCallCollectionAllocation*> allocation_ptrs;      // for bookkeeping purposes
    Array<DrawCallCollectionAllocation*> prev_allocation_ptrs; // for bookkeeping purposes
};

static FrameData g_frame_data[num_frames];

HYP_DISABLE_OPTIMIZATION;

HYP_API uint32 GetRenderThreadFrameIndex()
{
    return g_consumer_index.load(std::memory_order_relaxed);
}

HYP_API uint32 GetGameThreadFrameIndex()
{
    return g_producer_index.load(std::memory_order_relaxed);
}

HYP_API RenderProxyList& GetProducerRenderProxyList(View* view)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);

    uint32_t slot = g_producer_index.load(std::memory_order_relaxed);

    ViewData*& vd = g_frame_data[slot].views[view];

    if (!vd)
    {
        // create a new pool for this view
        vd = &g_frame_data[slot].per_view_data.EmplaceBack();
    }

    // reset the frames_since_write counter
    vd->frames_since_write = 0;

    return vd->render_proxy_list;
}

HYP_API RenderProxyList& GetConsumerRenderProxyList(View* view)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);

    uint32_t slot = g_consumer_index.load(std::memory_order_relaxed);

    ViewData*& vd = g_frame_data[slot].views[view];

    if (!vd)
    {
        // create a new pool for this view
        vd = &g_frame_data[slot].per_view_data.EmplaceBack();
    }

    return vd->render_proxy_list;
}

HYP_API DrawCallCollection* AllocateDrawCallCollection(View* view)
{
    HYP_SCOPE;

    HYP_NOT_IMPLEMENTED();

    // AssertDebug(view != nullptr);

    // uint32_t slot = g_producer_index.load(std::memory_order_relaxed);

    // ViewData*& vd = g_frame_data[slot].views[view];

    // if (!vd)
    // {
    //     // create a new pool for this view
    //     vd = &g_frame_data[slot].per_view_data.EmplaceBack();
    // }

    // uint32 idx;

    // DrawCallCollectionAllocation* allocation;
    // idx = vd->pool.AcquireIndex(&allocation);

    // if (idx == ~0u)
    // {
    //     HYP_FAIL("Failed to acquire draw collection!");
    // }

    // DrawCallCollection* collection;

    // if (allocation->alive_frames != 0)
    // {
    //     DebugLog(LogType::Debug, "Reusing draw call collection index %u from frame %u\n", allocation->index, allocation->index);

    //     // reuse the existing instance
    //     collection = allocation->storage.GetPointer();
    // }
    // else
    // {
    //     allocation->index = idx;
    //     allocation->alive_frames = 1;

    //     DebugLog(LogType::Debug, "Constructing draw call collection index %u from frame %u\n", allocation->index, allocation->index);

    //     collection = allocation->storage.Construct();
    // }

    // g_frame_data[slot].allocation_ptrs.PushBack(allocation);

    // return collection;
}

HYP_API void BeginFrame_GameThread()
{
    HYP_SCOPE;

    g_free_semaphore.acquire();
    uint32_t slot = g_producer_index.load(std::memory_order_relaxed);
}

HYP_API void EndFrame_GameThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_game_thread);
#endif

    g_producer_index.store((g_producer_index.load(std::memory_order_relaxed) + 1) % num_frames, std::memory_order_relaxed);
    g_frame_counter.fetch_add(1, std::memory_order_release); // publish the new frame #

    g_full_semaphore.release(); // a frame is ready for the renderer
}

HYP_API void BeginFrame_RenderThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_render_thread);
#endif

    // @TODO : read the draw collections from the previous frame using g_frame_data[frame_index].draw_collection_ptrs

    g_full_semaphore.acquire();

    uint32_t slot = g_consumer_index.load(std::memory_order_relaxed);
}

HYP_API void EndFrame_RenderThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_render_thread);
#endif

    uint32 slot = g_consumer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End();)
    {
        ViewData& vd = *it;

        // Clear out data for views that haven't been written to for a while
        if (vd.frames_since_write == 15)
        {
            DebugLog(LogType::Debug, "Clearing draw collection for view %p in frame %u\n", &vd.render_proxy_list, slot);

            // clear the draw collection for this view
            for (auto alloc_it = vd.allocation_ptrs.Begin(); alloc_it != vd.allocation_ptrs.End(); ++alloc_it)
            {
                DrawCallCollectionAllocation* allocation = *alloc_it;

                DebugLog(LogType::Debug, "Releasing draw collection index %u from frame %u\n", allocation->index, slot);

                allocation->storage.Destruct();
                allocation->alive_frames = 0;

                vd.pool.ReleaseIndex(allocation->index);
            }

            vd.allocation_ptrs.Clear();
            vd.prev_allocation_ptrs.Clear();

            auto view_it = frame_data.views.FindIf([&vd](const auto& pair)
                {
                    return pair.second == &vd;
                });

            AssertDebug(view_it != frame_data.views.End());

            frame_data.views.Erase(view_it);

            it = frame_data.per_view_data.Erase(it);

            continue;
        }

        vd.pool.ClearUsedIndices();

        for (DrawCallCollectionAllocation* allocation : vd.allocation_ptrs)
        {
            AssertDebug(allocation->alive_frames > 0);

            // Decrease the alive frames for all allocations
            --allocation->alive_frames;
        }

        for (auto alloc_it = vd.prev_allocation_ptrs.Begin(); alloc_it != vd.prev_allocation_ptrs.End();)
        {
            DrawCallCollectionAllocation* allocation = *alloc_it;

            ++allocation->alive_frames;

            if (allocation->alive_frames > 3)
            {
                allocation->storage.Destruct();
                allocation->alive_frames = 0;

                DebugLog(LogType::Debug, "Releasing draw collection index %u from frame %u\n", allocation->index, slot);

                vd.pool.ReleaseIndex(allocation->index);

                alloc_it = vd.prev_allocation_ptrs.Erase(alloc_it);
            }
            else
            {
                ++alloc_it;
            }
        }

        vd.prev_allocation_ptrs.Clear();
        std::swap(vd.allocation_ptrs, vd.prev_allocation_ptrs);

        ++vd.frames_since_write;

        ++it;
    }

    /// @TODO Some heuristic for clearing allocations that weren't active for a few frames

    // update the GPU buffers for the current frame @TODO !!

    g_consumer_index.store((slot + 1) % num_frames, std::memory_order_relaxed);

    g_free_semaphore.release();
}

HYP_ENABLE_OPTIMIZATION;

RenderGlobalState::RenderGlobalState()
    : ShadowMapAllocator(MakeUnique<class ShadowMapAllocator>()),
      GPUBufferHolderMap(MakeUnique<class GPUBufferHolderMap>()),
      PlaceholderData(MakeUnique<class PlaceholderData>())
{
    Worlds = GPUBufferHolderMap->GetOrCreate<WorldShaderData, GPUBufferType::CONSTANT_BUFFER>();
    Cameras = GPUBufferHolderMap->GetOrCreate<CameraShaderData, GPUBufferType::CONSTANT_BUFFER>();
    Lights = GPUBufferHolderMap->GetOrCreate<LightShaderData, GPUBufferType::STORAGE_BUFFER>();
    Entities = GPUBufferHolderMap->GetOrCreate<EntityShaderData, GPUBufferType::STORAGE_BUFFER>();
    Materials = GPUBufferHolderMap->GetOrCreate<MaterialShaderData, GPUBufferType::STORAGE_BUFFER>();
    Skeletons = GPUBufferHolderMap->GetOrCreate<SkeletonShaderData, GPUBufferType::STORAGE_BUFFER>();
    ShadowMaps = GPUBufferHolderMap->GetOrCreate<ShadowMapShaderData, GPUBufferType::STORAGE_BUFFER>();
    EnvProbes = GPUBufferHolderMap->GetOrCreate<EnvProbeShaderData, GPUBufferType::STORAGE_BUFFER>();
    EnvGrids = GPUBufferHolderMap->GetOrCreate<EnvGridShaderData, GPUBufferType::CONSTANT_BUFFER>();
    LightmapVolumes = GPUBufferHolderMap->GetOrCreate<LightmapVolumeShaderData, GPUBufferType::STORAGE_BUFFER>();

    GlobalDescriptorTable = g_rendering_api->MakeDescriptorTable(&renderer::GetStaticDescriptorTableDeclaration());

    PlaceholderData->Create();
    ShadowMapAllocator->Initialize();

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        SetDefaultDescriptorSetElements(frame_index);
    }

    CreateSphereSamplesBuffer();
    CreateBlueNoiseBuffer();

    GlobalDescriptorTable->Create();

    BindlessTextures.Create();
}

RenderGlobalState::~RenderGlobalState()
{
    BindlessTextures.Destroy();
    ShadowMapAllocator->Destroy();
    GlobalDescriptorTable->Destroy();
    PlaceholderData->Destroy();
}

void RenderGlobalState::UpdateBuffers(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    for (auto& it : GPUBufferHolderMap->GetItems())
    {
        it.second->UpdateBufferSize(frame->GetFrameIndex());
        it.second->UpdateBufferData(frame->GetFrameIndex());
    }
}

uint32 RenderGlobalState::AllocateIndex(IndexAllocatorType type)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(type + 1 <= IndexAllocatorType::MAX);

    uint32 index = m_index_allocators[type].AllocateIndex(index_allocator_maximums[type]);

    if (index == ~0u)
    {
        HYP_LOG(Rendering, Error, "Failed to allocate index for type {}. Maximum index limit reached: {}", type, index_allocator_maximums[type]);
    }

    return index;
}

void RenderGlobalState::FreeIndex(IndexAllocatorType type, uint32 index)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(type < IndexAllocatorType::MAX);

    if (index >= index_allocator_maximums[type])
    {
        HYP_LOG(Rendering, Error, "Attempted to free index {} for type {}, but it exceeds the maximum index limit: {}", index, type, index_allocator_maximums[type]);
        return;
    }

    m_index_allocators[type].FreeIndex(index);
}

void RenderGlobalState::CreateBlueNoiseBuffer()
{
    HYP_SCOPE;

    static_assert(sizeof(BlueNoiseBuffer::sobol_256spp_256d) == sizeof(BlueNoise::sobol_256spp_256d));
    static_assert(sizeof(BlueNoiseBuffer::scrambling_tile) == sizeof(BlueNoise::scrambling_tile));
    static_assert(sizeof(BlueNoiseBuffer::ranking_tile) == sizeof(BlueNoise::ranking_tile));

    constexpr SizeType blue_noise_buffer_size = sizeof(BlueNoiseBuffer);

    constexpr SizeType sobol_256spp_256d_offset = offsetof(BlueNoiseBuffer, sobol_256spp_256d);
    constexpr SizeType sobol_256spp_256d_size = sizeof(BlueNoise::sobol_256spp_256d);
    constexpr SizeType scrambling_tile_offset = offsetof(BlueNoiseBuffer, scrambling_tile);
    constexpr SizeType scrambling_tile_size = sizeof(BlueNoise::scrambling_tile);
    constexpr SizeType ranking_tile_offset = offsetof(BlueNoiseBuffer, ranking_tile);
    constexpr SizeType ranking_tile_size = sizeof(BlueNoise::ranking_tile);

    static_assert(blue_noise_buffer_size == (sobol_256spp_256d_offset + sobol_256spp_256d_size) + ((scrambling_tile_offset - (sobol_256spp_256d_offset + sobol_256spp_256d_size)) + scrambling_tile_size) + ((ranking_tile_offset - (scrambling_tile_offset + scrambling_tile_size)) + ranking_tile_size));

    GPUBufferRef blue_noise_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STORAGE_BUFFER, sizeof(BlueNoiseBuffer));
    HYPERION_ASSERT_RESULT(blue_noise_buffer->Create());
    blue_noise_buffer->Copy(sobol_256spp_256d_offset, sobol_256spp_256d_size, &BlueNoise::sobol_256spp_256d[0]);
    blue_noise_buffer->Copy(scrambling_tile_offset, scrambling_tile_size, &BlueNoise::scrambling_tile[0]);
    blue_noise_buffer->Copy(ranking_tile_offset, ranking_tile_size, &BlueNoise::ranking_tile[0]);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("BlueNoiseBuffer"), blue_noise_buffer);
    }
}

void RenderGlobalState::CreateSphereSamplesBuffer()
{
    HYP_SCOPE;

    GPUBufferRef sphere_samples_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(Vec4f) * 4096);
    HYPERION_ASSERT_RESULT(sphere_samples_buffer->Create());

    Vec4f* sphere_samples = new Vec4f[4096];

    uint32 seed = 0;

    for (uint32 i = 0; i < 4096; i++)
    {
        Vec3f sample = MathUtil::RandomInSphere(Vec3f {
            MathUtil::RandomFloat(seed),
            MathUtil::RandomFloat(seed),
            MathUtil::RandomFloat(seed) });

        sphere_samples[i] = Vec4f(sample, 0.0f);
    }

    sphere_samples_buffer->Copy(sizeof(Vec4f) * 4096, sphere_samples);

    delete[] sphere_samples;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("SphereSamplesBuffer"), sphere_samples_buffer);
    }
}

void RenderGlobalState::SetDefaultDescriptorSetElements(uint32 frame_index)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    // Global
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("WorldsBuffer"), Worlds->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightsBuffer"), Lights->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CurrentLight"), Lights->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ObjectsBuffer"), Entities->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CamerasBuffer"), Cameras->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvGridsBuffer"), EnvGrids->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbesBuffer"), EnvProbes->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CurrentEnvProbe"), EnvProbes->GetBuffer(frame_index));

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("VoxelGridTexture"), PlaceholderData->GetImageView3D1x1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightFieldColorTexture"), PlaceholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightFieldDepthTexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("BlueNoiseBuffer"), GPUBufferRef::Null());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SphereSamplesBuffer"), GPUBufferRef::Null());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsTextureArray"), PlaceholderData->GetImageView2D1x1R8Array());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PointLightShadowMapsTextureArray"), PlaceholderData->GetImageViewCube1x1R8Array());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsBuffer"), ShadowMaps->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightmapVolumesBuffer"), LightmapVolumes->GetBuffer(frame_index));

    for (uint32 i = 0; i < max_bound_reflection_probes; i++)
    {
        GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), i, PlaceholderData->DefaultTexture2D->GetRenderResource().GetImageView());
    }

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIUniforms"), PlaceholderData->GetOrCreateBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(DDGIUniforms), true /* exact size */));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIIrradianceTexture"), PlaceholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIDepthTexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("RTRadianceResultTexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SamplerNearest"), PlaceholderData->GetSamplerNearest());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SamplerLinear"), PlaceholderData->GetSamplerLinearMipmap());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("UITexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("FinalOutputTexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsTextureArray"), ShadowMapAllocator->GetAtlasImageView());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PointLightShadowMapsTextureArray"), ShadowMapAllocator->GetPointLightShadowMapImageView());

    // Object
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("CurrentObject"), Entities->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("MaterialsBuffer"), Materials->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("SkeletonsBuffer"), Skeletons->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("LightmapVolumeIrradianceTexture"), PlaceholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("LightmapVolumeRadianceTexture"), PlaceholderData->GetImageView2D1x1R8());

    // Material
    if (g_rendering_api->GetRenderConfig().IsBindlessSupported())
    {
        for (uint32 texture_index = 0; texture_index < max_bindless_resources; texture_index++)
        {
            GlobalDescriptorTable->GetDescriptorSet(NAME("Material"), frame_index)
                ->SetElement(NAME("Textures"), texture_index, PlaceholderData->GetImageView2D1x1R8());
        }
    }
}

} // namespace hyperion
