/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Handle.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region ShadowMapAtlas

ShadowMapAtlas::ShadowMapAtlas(uint32 atlas_index, const Vec2u &atlas_dimensions)
    : atlas_index(atlas_index),
      atlas_dimensions(atlas_dimensions)
{
    free_spaces.PushBack(SkylineNode { Vec2i::Zero(), Vec2i { int(atlas_dimensions.x), 0 } });
}

bool ShadowMapAtlas::AddElement(const Vec2u &element_dimensions, ShadowMapAtlasElement &out_element)
{
    int best_y = INT32_MAX;
    int best_x = -1;
    int best_index = -1;

    for (SizeType i = 0; i < free_spaces.Size(); i++) {
        Vec2u offset;

        if (CalculateFitOffset(uint32(i), element_dimensions, offset)) {
            if (int(offset.y) < best_y) {
                best_x = int(offset.x);
                best_y = int(offset.y);
                best_index = int(i);
            }
        }
    }

    if (best_index != -1) {
        out_element.atlas_index = atlas_index;
        out_element.offset_coords = Vec2u { uint32(best_x), uint32(best_y) };
        out_element.offset_uv = Vec2f(out_element.offset_coords) / Vec2f(atlas_dimensions);
        out_element.dimensions = element_dimensions;
        out_element.scale = Vec2f(element_dimensions) / Vec2f(atlas_dimensions);

        AddSkylineNode(uint32(best_index), Vec2u { element_dimensions.x, 0 }, Vec2u { uint32(best_x), uint32(best_y) + element_dimensions.y });

        return true;
    }

    return false;
}

bool ShadowMapAtlas::RemoveElement(const ShadowMapAtlasElement &element)
{
    auto it = elements.Find(element);

    if (it == elements.End()) {
        return false;
    }

    free_spaces.PushBack(SkylineNode { Vec2i(element.offset_uv * Vec2f(atlas_dimensions)), Vec2i(element.dimensions) });

    std::sort(free_spaces.Begin(), free_spaces.End(), [](const SkylineNode &a, const SkylineNode &b)
    {
        return a.offset.x < b.offset.x;
    });

    MergeSkyline();

    elements.Erase(it);

    return true;
}

void ShadowMapAtlas::Clear()
{
    free_spaces.Clear();
    elements.Clear();

    // Add the initial skyline node
    free_spaces.PushBack(SkylineNode { Vec2i::Zero(), Vec2i { int(atlas_dimensions.x), 0 } });
}

// Based on: https://jvernay.fr/en/blog/skyline-2d-packer/implementation/
bool ShadowMapAtlas::CalculateFitOffset(uint32 index, const Vec2u &dimensions, Vec2u &out_offset) const
{
    const int x = free_spaces[index].offset.x;
    int y = free_spaces[index].offset.y;

    int remaining_width = free_spaces[index].dimensions.x;

    if (x + dimensions.x > atlas_dimensions.x) {
        return false;
    }

    for (uint32 i = index; i < free_spaces.Size() && remaining_width > 0; i++) {
        const int node_y = free_spaces[i].offset.y;

        y = MathUtil::Max(y, node_y);

        if (y + dimensions.y > atlas_dimensions.y) {
            return false;
        }

        remaining_width -= free_spaces[i].dimensions.x;
    }

    out_offset = Vec2u { uint32(x), uint32(y) };

    return true;
}

void ShadowMapAtlas::AddSkylineNode(uint32 before_index, const Vec2u &dimensions, const Vec2u &offset)
{
    free_spaces.Insert(free_spaces.Begin() + before_index, SkylineNode { Vec2i(offset), Vec2i(dimensions) });

    for (SizeType i = before_index + 1; i < free_spaces.Size(); i++) {
        if (free_spaces[i].offset.x < offset.x + dimensions.x) {
            int shrink = offset.x + dimensions.x - free_spaces[i].offset.x;

            free_spaces[i].offset.x += shrink;
            free_spaces[i].dimensions.x -= shrink;

            if (free_spaces[i].dimensions.x <= 0) {
                free_spaces.EraseAt(i);
            } else {
                break;
            }
        } else {
            break;
        }
    }

    MergeSkyline();
}

void ShadowMapAtlas::MergeSkyline()
{
    for (SizeType i = 0; i < free_spaces.Size() - 1;) {
        int y0 = free_spaces[i].offset.y;
        int y1 = free_spaces[i + 1].offset.y;

        if (y0 == y1) {
            free_spaces[i].dimensions.x += free_spaces[i + 1].dimensions.x;
            free_spaces.EraseAt(i + 1);
        } else {
            i++;
        }
    }
}

#pragma endregion ShadowMapAtlas

#pragma region ShadowMapManager

ShadowMapManager::ShadowMapManager()
    : m_atlas_dimensions(2048, 2048)
{
    m_atlases.Reserve(max_shadow_maps);

    for (SizeType i = 0; i < max_shadow_maps; i++) {
        m_atlases.PushBack(ShadowMapAtlas(uint32(i), m_atlas_dimensions));
    }
}

ShadowMapManager::~ShadowMapManager()
{
    SafeRelease(std::move(m_atlas_image));
    SafeRelease(std::move(m_atlas_image_view));

    SafeRelease(std::move(m_point_light_shadow_map_image));
    SafeRelease(std::move(m_point_light_shadow_map_image_view));
}

void ShadowMapManager::Initialize()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    m_atlas_image = g_rendering_api->MakeImage(TextureDesc {
        ImageType::TEXTURE_TYPE_2D_ARRAY,
        InternalFormat::RG32F,
        Vec3u { m_atlas_dimensions, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        uint32(m_atlases.Size()),
        ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE
    });

    HYPERION_ASSERT_RESULT(m_atlas_image->Create());

    m_atlas_image_view = g_rendering_api->MakeImageView(m_atlas_image);
    HYPERION_ASSERT_RESULT(m_atlas_image_view->Create());

    m_point_light_shadow_map_image = g_rendering_api->MakeImage(TextureDesc {
        ImageType::TEXTURE_TYPE_CUBEMAP_ARRAY,
        InternalFormat::RG32F,
        Vec3u { 512, 512, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        max_bound_point_shadow_maps * 6,
        ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE
    });

    HYPERION_ASSERT_RESULT(m_point_light_shadow_map_image->Create());

    m_point_light_shadow_map_image_view = g_rendering_api->MakeImageView(m_point_light_shadow_map_image);
    HYPERION_ASSERT_RESULT(m_point_light_shadow_map_image_view->Create());

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("ShadowMapsTextureArray"), m_atlas_image_view);

        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("PointLightShadowMapsTextureArray"), m_point_light_shadow_map_image_view);
    }
}

void ShadowMapManager::Destroy()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    for (ShadowMapAtlas &atlas : m_atlases) {
        atlas.Clear();
    }

    // unset the shadow map texture array in the global descriptor set
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("ShadowMapsTextureArray"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8Array());
        
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("PointLightShadowMapsTextureArray"), g_engine->GetPlaceholderData()->GetImageViewCube1x1R8Array());
    }

    SafeRelease(std::move(m_atlas_image));
    SafeRelease(std::move(m_atlas_image_view));

    SafeRelease(std::move(m_point_light_shadow_map_image));
    SafeRelease(std::move(m_point_light_shadow_map_image_view));
}

ShadowMapRenderResource *ShadowMapManager::AllocateShadowMap(ShadowMapType shadow_map_type, ShadowMapFilterMode filter_mode, const Vec2u &dimensions)
{
    if (shadow_map_type == ShadowMapType::POINT_SHADOW_MAP) {
        const uint32 point_light_index = m_point_light_shadow_map_id_generator.NextID() - 1;

        // Cannot allocate if we ran out of IDs
        if (point_light_index >= max_bound_point_shadow_maps) {
            m_point_light_shadow_map_id_generator.FreeID(point_light_index + 1);

            return nullptr;
        }

        const ShadowMapAtlasElement atlas_element {
            .atlas_index        = ~0u,
            .point_light_index  = point_light_index,
            .offset_coords      = Vec2u::Zero(),
            .offset_uv          = Vec2f::Zero(),
            .dimensions         = dimensions,
            .scale              = Vec2f::One()
        };

        ShadowMapRenderResource *shadow_map_render_resource = AllocateResource<ShadowMapRenderResource>(
            shadow_map_type,
            filter_mode,
            atlas_element,
            m_point_light_shadow_map_image_view
        );

        return shadow_map_render_resource;
    }

    for (ShadowMapAtlas &atlas : m_atlases) {
        ShadowMapAtlasElement atlas_element;

        if (atlas.AddElement(dimensions, atlas_element)) {
            ImageViewRef atlas_image_view = m_atlas_image->MakeLayerImageView(atlas_element.atlas_index);
            DeferCreate(atlas_image_view);

            ShadowMapRenderResource *shadow_map_render_resource = AllocateResource<ShadowMapRenderResource>(
                shadow_map_type,
                filter_mode,
                atlas_element,
                atlas_image_view
            );

            return shadow_map_render_resource;
        }
    }

    return nullptr;
}

bool ShadowMapManager::FreeShadowMap(ShadowMapRenderResource *shadow_map_render_resource)
{
    if (!shadow_map_render_resource) {
        return false;
    }

    const ShadowMapAtlasElement &atlas_element = shadow_map_render_resource->GetAtlasElement();

    bool result = false;

    if (atlas_element.atlas_index != ~0u) {
        AssertThrow(atlas_element.atlas_index < m_atlases.Size());

        ShadowMapAtlas &atlas = m_atlases[atlas_element.atlas_index];
        result = atlas.RemoveElement(atlas_element);
        
        if (!result) {
            HYP_LOG(Rendering, Error, "Failed to free shadow map from atlas (atlas index: {})", atlas_element.atlas_index);
        }
    } else if (atlas_element.point_light_index != ~0u) {
        m_point_light_shadow_map_id_generator.FreeID(atlas_element.point_light_index + 1);

        result = true;
    } else {
        HYP_LOG(Rendering, Error, "Failed to free shadow map: invalid atlas index and point light index");
    }

    FreeResource(shadow_map_render_resource);

    return result;
}

#pragma endregion ShadowMapManager

#pragma region WorldRenderResource

WorldRenderResource::WorldRenderResource(World *world)
    : m_world(world),
      m_shadow_map_manager(MakeUnique<ShadowMapManager>())
{
}

WorldRenderResource::~WorldRenderResource() = default;

void WorldRenderResource::AddView(TResourceHandle<ViewRenderResource> &&view_render_resource_handle)
{
    HYP_SCOPE;

    if (!view_render_resource_handle) {
        return;
    }

    Execute([this, view_render_resource_handle = std::move(view_render_resource_handle)]()
    {
        m_view_render_resource_handles.PushBack(std::move(view_render_resource_handle));

        std::sort(m_view_render_resource_handles.Begin(), m_view_render_resource_handles.End(), [](const TResourceHandle<ViewRenderResource> &a, const TResourceHandle<ViewRenderResource> &b)
        {
            return uint32(b->GetPriority()) < uint32(a->GetPriority());
        });
    }, /* force_owner_thread */ true);
}

void WorldRenderResource::RemoveView(ViewRenderResource *view_render_resource)
{
    HYP_SCOPE;

    if (!view_render_resource) {
        return;
    }

    Execute([this, view_render_resource]()
    {
        auto it = m_view_render_resource_handles.FindIf([view_render_resource](const TResourceHandle<ViewRenderResource> &item)
        {
            return item.Get() == view_render_resource;
        });

        if (it != m_view_render_resource_handles.End()) {
            ViewRenderResource *view_render_resource = it->Get();
            it->Reset();

            m_view_render_resource_handles.Erase(it);
        }
    }, /* force_owner_thread */ true);
}

void WorldRenderResource::RemoveViewsForScene(const WeakHandle<Scene> &scene_weak)
{
    HYP_SCOPE;

    if (!scene_weak.IsValid()) {
        return;
    }

    Execute([this, scene_weak]()
    {
        for (auto it = m_view_render_resource_handles.Begin(); it != m_view_render_resource_handles.End();) {
            if (!(*it)->GetScene()) {
                ++it;

                continue;
            }

            if ((*it)->GetScene()->GetScene() == scene_weak.GetUnsafe()) {
                ViewRenderResource *view_render_resource = it->Get();

                it->Reset();

                it = m_view_render_resource_handles.Erase(it);
            } else {
                ++it;
            }
        }
    }, /* force_owner_thread */ true);
}

void WorldRenderResource::AddScene(TResourceHandle<SceneRenderResource> &&scene_render_resource_handle)
{
    HYP_SCOPE;

    if (!scene_render_resource_handle) {
        return;
    }

    Execute([this, scene_render_resource_handle = std::move(scene_render_resource_handle)]()
    {
        m_scene_render_resource_handles.PushBack(std::move(scene_render_resource_handle));
    }, /* force_owner_thread */ true);
}

void WorldRenderResource::RemoveScene(SceneRenderResource *scene_render_resource)
{
    HYP_SCOPE;

    if (!scene_render_resource) {
        return;
    }

    Execute([this, scene_render_resource]()
    {
        auto it = m_scene_render_resource_handles.FindIf([scene_render_resource](const TResourceHandle<SceneRenderResource> &item)
        {
            return item.Get() == scene_render_resource;
        });

        if (it != m_scene_render_resource_handles.End()) {
            m_scene_render_resource_handles.Erase(it);
        }
    }, /* force_owner_thread */ true);
}

// void WorldRenderResource::AddShadowMapRenderResource(const TResourceHandle<ShadowMapRenderResource> &shadow_map_resource_handle)
// {
//     HYP_SCOPE;

//     if (!shadow_map_resource_handle) {
//         return;
//     }

//     Execute([this, shadow_map_resource_handle]()
//     {
//         m_shadow_map_resource_handles.PushBack(std::move(shadow_map_resource_handle));
//     }, /* force_owner_thread */ true);
// }

// void WorldRenderResource::RemoveShadowMapRenderResource(const ShadowMapRenderResource *shadow_map_render_resource)
// {
//     HYP_SCOPE;

//     if (!shadow_map_render_resource) {
//         return;
//     }

//     Execute([this, shadow_map_render_resource]()
//     {
//         auto it = m_shadow_map_resource_handles.FindIf([shadow_map_render_resource](const TResourceHandle<ShadowMapRenderResource> &item)
//         {
//             return item.Get() == shadow_map_render_resource;
//         });

//         if (it != m_shadow_map_resource_handles.End()) {
//             m_shadow_map_resource_handles.Erase(it);
//         }
//     }, /* force_owner_thread */ true);
// }

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

    m_shadow_map_manager->Initialize();
}

void WorldRenderResource::Destroy_Internal()
{
    HYP_SCOPE;

    for (TResourceHandle<ViewRenderResource> &view_render_resource_handle : m_view_render_resource_handles) {
        ViewRenderResource *view_render_resource = view_render_resource_handle.Get();

        view_render_resource_handle.Reset();
    }

    m_view_render_resource_handles.Clear();

    m_shadow_map_manager->Destroy();
}

void WorldRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *WorldRenderResource::GetGPUBufferHolder() const
{
    return nullptr;
}

void WorldRenderResource::PreRender(FrameBase *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    for (const TResourceHandle<ViewRenderResource> &current_view : m_view_render_resource_handles) {
        current_view->PreFrameUpdate(frame);
    }
}

void WorldRenderResource::Render(FrameBase *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    for (const TResourceHandle<SceneRenderResource> &scene_render_resource_handle : m_scene_render_resource_handles) {
        scene_render_resource_handle->GetEnvironment()->RenderSubsystems(frame);
    }

    for (const TResourceHandle<ViewRenderResource> &current_view : m_view_render_resource_handles) {
        current_view->Render(frame, this);
    }
}

#pragma endregion WorldRenderResource

} // namespace hyperion