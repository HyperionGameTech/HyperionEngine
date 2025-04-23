/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/Buffers.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Handle.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region WorldRenderResource

WorldRenderResource::WorldRenderResource(World *world)
    : m_world(world)
{
    m_environment = CreateObject<RenderEnvironment>();
    InitObject(m_environment);
}

WorldRenderResource::~WorldRenderResource() = default;

void WorldRenderResource::AddScene(const Handle<Scene> &scene)
{
    HYP_SCOPE;

    if (!scene.IsValid()) {
        return;
    }

    Execute([this, scene_weak = scene.ToWeak()]()
    {
        if (Handle<Scene> scene = scene_weak.Lock()) {
            m_bound_scenes.PushBack(TResourceHandle<SceneRenderResource>(scene->GetRenderResource()));
        }
    }, /* force_owner_thread */ true);          
}

Task<bool> WorldRenderResource::RemoveScene(const WeakHandle<Scene> &scene_weak)
{
    HYP_SCOPE;

    Task<bool> task;
    auto task_executor = task.Initialize();

    if (!scene_weak.IsValid()) {
        task_executor->Fulfill(false);

        return task;
    }

    Execute([this, scene_weak, task_executor]()
    {
        const ID<Scene> scene_id = scene_weak.GetID();

        auto bound_scenes_it = m_bound_scenes.FindIf([&scene_id](const TResourceHandle<SceneRenderResource> &item)
        {
            return item->GetScene()->GetID() == scene_id;
        });

        if (bound_scenes_it != m_bound_scenes.End()) {
            m_bound_scenes.Erase(bound_scenes_it);

            task_executor->Fulfill(true);
        } else {
            task_executor->Fulfill(false);
        }
    }, /* force_owner_thread */ true);

    return task;
}

void WorldRenderResource::AddShadowMapRenderResource(const TResourceHandle<ShadowMapRenderResource> &shadow_map_resource_handle)
{
    HYP_SCOPE;

    if (!shadow_map_resource_handle) {
        return;
    }

    Execute([this, shadow_map_resource_handle]()
    {
        m_shadow_map_resource_handles.PushBack(std::move(shadow_map_resource_handle));
    }, /* force_owner_thread */ true);
}

void WorldRenderResource::RemoveShadowMapRenderResource(const ShadowMapRenderResource *shadow_map_render_resource)
{
    HYP_SCOPE;

    if (!shadow_map_render_resource) {
        return;
    }

    Execute([this, shadow_map_render_resource]()
    {
        auto it = m_shadow_map_resource_handles.FindIf([shadow_map_render_resource](const TResourceHandle<ShadowMapRenderResource> &item)
        {
            return item.Get() == shadow_map_render_resource;
        });

        if (it != m_shadow_map_resource_handles.End()) {
            m_shadow_map_resource_handles.Erase(it);
        }
    }, /* force_owner_thread */ true);
}

const EngineRenderStats &WorldRenderResource::GetRenderStats() const
{
    HYP_SCOPE;
    if (Threads::IsOnThread(g_render_thread)) {
        return m_render_stats[ThreadType::THREAD_TYPE_RENDER];
    } else if (Threads::IsOnThread(g_game_thread)) {
        return m_render_stats[ThreadType::THREAD_TYPE_GAME];
    } else {
        HYP_UNREACHABLE();
    }
}

void WorldRenderResource::SetRenderStats(const EngineRenderStats &render_stats)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    m_render_stats[ThreadType::THREAD_TYPE_GAME] = render_stats;

    Execute([this, render_stats]()
    {
        m_render_stats[ThreadType::THREAD_TYPE_RENDER] = render_stats;
    });
}

void WorldRenderResource::Initialize_Internal()
{
    HYP_SCOPE;
    
    CreateShadowMapsTextureArray();
}

void WorldRenderResource::Destroy_Internal()
{
    HYP_SCOPE;

    m_bound_scenes.Clear();

    SafeRelease(std::move(m_shadow_maps_texture_array_image));
    SafeRelease(std::move(m_shadow_maps_texture_array_image_view));
}

void WorldRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *WorldRenderResource::GetGPUBufferHolder() const
{
    return nullptr;
}

void WorldRenderResource::PreRender(renderer::FrameBase *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
}

void WorldRenderResource::PostRender(renderer::FrameBase *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
}

void WorldRenderResource::Render(renderer::FrameBase *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    m_environment->RenderSubsystems(frame);
}

void WorldRenderResource::CreateShadowMapsTextureArray()
{
    HYP_SCOPE;

    m_shadow_maps_texture_array_image = g_rendering_api->MakeImage(TextureDesc {
        ImageType::TEXTURE_TYPE_2D_ARRAY,
        InternalFormat::RG32F,
        Vec3u { 2048, 2048, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        uint32(max_shadow_maps),
        ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE
    });

    HYPERION_ASSERT_RESULT(m_shadow_maps_texture_array_image->Create());

    m_shadow_maps_texture_array_image_view = g_rendering_api->MakeImageView(m_shadow_maps_texture_array_image);
    HYPERION_ASSERT_RESULT(m_shadow_maps_texture_array_image_view->Create());

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
            ->SetElement(NAME("ShadowMapsTextureArray"), m_shadow_maps_texture_array_image_view);
    }
}

#pragma endregion WorldRenderResource

} // namespace hyperion