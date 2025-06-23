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
#include <rendering/Renderer.hpp>
#include <rendering/Deferred.hpp>
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
#include <scene/EnvProbe.hpp>

#include <core/object/HypClass.hpp>

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
static constexpr uint32 max_frames_before_discard = 4; // number of frames before ViewData is discarded if not written to

// global ring buffer for the game and render threads to write/read data from
static std::atomic_uint g_producer_index { 0 };                             // where the game will write next
static std::atomic_uint g_consumer_index { 0 };                             // what the renderer is *about* to draw
static std::atomic_uint g_frame_counter { 0 };                              // logical frame number
static std::counting_semaphore<num_frames> g_full_semaphore { 0 };          // renderer waits here
static std::counting_semaphore<num_frames> g_free_semaphore { num_frames }; // game waits here when ring is full

// Shared allocator for reflection probes and sky probes.
static ObjectBindingAllocator<16> g_envprobe_bindings_allocator;

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
    uint32 frames_since_write : 4; // frames_since_write of this view in the current frame, used to determine if the view is still alive if not reset to zero.
};

struct FrameData
{
    LinkedList<ViewData> per_view_data;
    HashMap<View*, ViewData*> views;

    // FixedArray<ViewData*, max_views_per_frame> views;
    // uint32 view_id_counter = 0;
};

static FrameData g_frame_data[num_frames];

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

    const uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    ViewData*& vd = g_frame_data[slot].views[view];

    if (!vd)
    {
        // create a new pool for this view
        vd = &g_frame_data[slot].per_view_data.EmplaceBack();
        vd->render_proxy_list.state = RenderProxyList::CS_WRITING;
    }

    // reset the frames_since_write counter
    vd->frames_since_write = 0;

    return vd->render_proxy_list;
}

HYP_API RenderProxyList& GetConsumerRenderProxyList(View* view)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);

    const uint32 slot = g_consumer_index.load(std::memory_order_relaxed);

    ViewData*& vd = g_frame_data[slot].views[view];

    if (!vd)
    {
        // create a new pool for this view
        vd = &g_frame_data[slot].per_view_data.EmplaceBack();
        vd->render_proxy_list.state = RenderProxyList::CS_READING;
    }

    return vd->render_proxy_list;
}

HYP_API void BeginFrame_GameThread()
{
    HYP_SCOPE;

    g_free_semaphore.acquire();

    uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End(); ++it)
    {
        ViewData& vd = *it;

        vd.render_proxy_list.BeginWrite();
    }
}

HYP_API void EndFrame_GameThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_game_thread);
#endif

    uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End(); ++it)
    {
        ViewData& vd = *it;

        vd.render_proxy_list.EndWrite();
    }

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

    g_full_semaphore.acquire();

    uint32 slot = g_consumer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End(); ++it)
    {
        ViewData& vd = *it;
        vd.render_proxy_list.BeginRead();
    }

    // Reset binding allocators at the end of the frame
    g_envprobe_bindings_allocator.Reset();
}

HYP_API void EndFrame_RenderThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_render_thread);
#endif

    uint32 slot = g_consumer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    // cull ViewData that hasn't been written to for a while
    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End();)
    {
        ViewData& vd = *it;
        vd.render_proxy_list.EndRead();
        vd.render_proxy_list.RemoveEmptyRenderGroups();

        // Clear out data for views that haven't been written to for a while
        if (vd.frames_since_write == max_frames_before_discard)
        {
            DebugLog(LogType::Debug, "Clearing draw collection for view %p in frame %u\n", &vd.render_proxy_list, slot);

            auto view_it = frame_data.views.FindIf([&vd](const auto& pair)
                {
                    return pair.second == &vd;
                });

            AssertDebug(view_it != frame_data.views.End());

            frame_data.views.Erase(view_it);

            it = frame_data.per_view_data.Erase(it);

            /// TODO: Clear tracked resources - DecRef needs to be called on all resources in the render_proxy_list

            continue;
        }

        ++it;
    }

    g_consumer_index.store((slot + 1) % num_frames, std::memory_order_relaxed);

    g_free_semaphore.release();
}

#pragma region ObjectBinderBase

ObjectBinderBase::ObjectBinderBase(RenderGlobalState* rgs)
{
    AssertDebug(rgs != nullptr);

    for (uint32 i = 0; i < RenderGlobalState::max_binders; i++)
    {
        if (rgs->ObjectBinders[i] == nullptr)
        {
            rgs->ObjectBinders[i] = this;
            return;
        }
    }

    HYP_FAIL("Failed to find a free slot in the RenderGlobalState's ObjectBinders array!");
}

int ObjectBinderBase::GetSubclassIndex(TypeID base_type_id, TypeID subclass_type_id)
{
    const HypClass* base = GetClass(base_type_id);
    if (!base)
    {
        return -1;
    }

    const HypClass* subclass = GetClass(subclass_type_id);

    if (!subclass)
    {
        return -1;
    }

    int subclass_static_index = subclass->GetStaticIndex();
    if (subclass_static_index < 0)
    {
        return -1; // subclass is not a static class
    }

    return (subclass_static_index - base->GetStaticIndex()) <= base->GetNumDescendants();
}

SizeType ObjectBinderBase::GetNumDescendants(TypeID type_id)
{
    const HypClass* base = GetClass(type_id);
    if (!base)
    {
        return 0;
    }

    return base->GetNumDescendants();
}

#pragma endregion ObjectBinderBase

#pragma region RenderGlobalState

RenderGlobalState::RenderGlobalState()
    : ObjectBinders { nullptr },
      EnvProbeBinder(this, &g_envprobe_bindings_allocator),
      ShadowMapAllocator(MakeUnique<class ShadowMapAllocator>()),
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

    Renderer = new DeferredRenderer();
    Renderer->Initialize();

    EnvProbeRenderers = new EnvProbeRenderer*[EPT_MAX];
    Memory::MemSet(EnvProbeRenderers, 0, sizeof(EnvProbeRenderer*) * EPT_MAX);
    EnvProbeRenderers[EPT_REFLECTION] = new ReflectionProbeRenderer();
    // EnvProbeRenderers[EPT_SKY] = new SkyEnvProbeRenderer();
}

RenderGlobalState::~RenderGlobalState()
{
    BindlessTextures.Destroy();
    ShadowMapAllocator->Destroy();
    GlobalDescriptorTable->Destroy();
    PlaceholderData->Destroy();

    for (uint32 i = 0; i < EPT_MAX; i++)
    {
        if (EnvProbeRenderers[i])
        {
            EnvProbeRenderers[i]->Shutdown();
            delete EnvProbeRenderers[i];
        }
    }

    delete[] EnvProbeRenderers;

    Renderer->Shutdown();
    delete Renderer;
    Renderer = nullptr;
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

void RenderGlobalState::OnEnvProbeBindingChanged(EnvProbe* env_probe, uint32 prev, uint32 next)
{
    AssertDebug(env_probe != nullptr);
    AssertDebug(env_probe->IsReady());

    DebugLog(LogType::Debug, "EnvProbe %u (class: %s) binding changed from %u to %u\n", env_probe->GetID().Value(),
        *env_probe->InstanceClass()->GetName(),
        prev, next);

    if (prev != ~0u)
    {
        HYP_LOG(Rendering, Debug, "UN setting env probe texture at index: {}", prev);
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), prev, g_render_global_state->PlaceholderData->DefaultTexture2D->GetRenderResource().GetImageView());
        }
    }
    else
    {
        env_probe->GetRenderResource().IncRef();
        env_probe->GetPrefilteredEnvMap()->GetRenderResource().IncRef();
    }

    // temp solution
    env_probe->GetRenderResource().SetTextureSlot(next);

    if (next != ~0u)
    {
        AssertDebug(env_probe->GetPrefilteredEnvMap().IsValid());
        AssertDebug(env_probe->GetPrefilteredEnvMap()->IsReady());

        HYP_LOG(Rendering, Debug, "Setting env probe texture at index: {} to tex with ID: {}", next, env_probe->GetPrefilteredEnvMap().GetID());
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), next, env_probe->GetPrefilteredEnvMap()->GetRenderResource().GetImageView());
        }
    }
    else
    {
        env_probe->GetRenderResource().DecRef();
        env_probe->GetPrefilteredEnvMap()->GetRenderResource().DecRef();
    }
}

#pragma endregion RenderGlobalState

} // namespace hyperion
