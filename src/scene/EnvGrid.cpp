/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/EnvGrid.hpp>
#include <scene/View.hpp>

#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderShadowMap.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <Engine.hpp>

namespace hyperion {

static const Vec2u sh_probe_dimensions { 256, 256 };
static const Vec2u light_field_probe_dimensions { 32, 32 };

static Vec2u GetProbeDimensions(EnvGridType env_grid_type)
{
    switch (env_grid_type)
    {
    case EnvGridType::ENV_GRID_TYPE_SH:
        return sh_probe_dimensions;
    case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
        return light_field_probe_dimensions;
    default:
        HYP_UNREACHABLE();
    }
}

#pragma region EnvProbeCollection

uint32 EnvProbeCollection::AddProbe(const Handle<EnvProbe>& env_probe)
{
    AssertThrow(env_probe.IsValid());
    AssertThrow(env_probe->IsReady());

    AssertThrow(num_probes < max_bound_ambient_probes);

    const uint32 index = num_probes++;

    env_probes[index] = env_probe;
    env_render_probes[index] = TResourceHandle<RenderEnvProbe>(env_probe->GetRenderResource());
    indirect_indices[index] = index;
    indirect_indices[max_bound_ambient_probes + index] = index;

    return index;
}

// Must be called in EnvGrid::Init(), before probes are used from the render thread.
void EnvProbeCollection::AddProbe(uint32 index, const Handle<EnvProbe>& env_probe)
{
    AssertThrow(env_probe.IsValid());
    AssertThrow(env_probe->IsReady());

    AssertThrow(index < max_bound_ambient_probes);

    num_probes = MathUtil::Max(num_probes, index + 1);

    env_probes[index] = env_probe;
    env_render_probes[index] = TResourceHandle<RenderEnvProbe>(env_probe->GetRenderResource());
    indirect_indices[index] = index;
    indirect_indices[max_bound_ambient_probes + index] = index;
}

#pragma region EnvProbeCollection

#pragma region EnvGrid

EnvGrid::EnvGrid()
    : EnvGrid(
          Handle<Scene> {},
          BoundingBox::Empty(),
          EnvGridOptions {})
{
}

EnvGrid::EnvGrid(
    const Handle<Scene>& parent_scene,
    const BoundingBox& aabb,
    const EnvGridOptions& options)
    : HypObject(),
      m_parent_scene(parent_scene),
      m_aabb(aabb),
      m_offset(aabb.GetCenter()),
      m_voxel_grid_aabb(aabb),
      m_options(options),
      m_render_resource(nullptr)
{
}

EnvGrid::~EnvGrid()
{
    if (m_render_resource != nullptr)
    {
        FreeResource(m_render_resource);

        m_render_resource = nullptr;
    }
}

void EnvGrid::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
        {
            if (m_render_resource != nullptr)
            {
                FreeResource(m_render_resource);

                m_render_resource = nullptr;
            }

            m_view.Reset();
            m_camera.Reset();
        }));

    AssertThrow(m_parent_scene.IsValid());

    const Vec2u probe_dimensions = GetProbeDimensions(m_options.type);
    AssertThrow(probe_dimensions.Volume() != 0);

    CreateEnvProbes();

    m_camera = CreateObject<Camera>(
        90.0f,
        -int(probe_dimensions.x), int(probe_dimensions.y),
        0.001f, m_aabb.GetRadius() * 2.0f);

    m_camera->SetName(Name::Unique("EnvGridCamera"));
    m_camera->SetTranslation(m_aabb.GetCenter());
    InitObject(m_camera);

    m_render_resource = AllocateResource<RenderEnvGrid>(this);

    m_view = CreateObject<View>(ViewDesc {
        .flags = ViewFlags::COLLECT_STATIC_ENTITIES
            | ViewFlags::SKIP_FRUSTUM_CULLING
            | ViewFlags::SKIP_ENV_PROBES
            | ViewFlags::SKIP_ENV_GRIDS,
        .viewport = Viewport { .extent = Vec2i(probe_dimensions), .position = Vec2i::Zero() },
        .scenes = { m_parent_scene },
        .camera = m_camera,
        .override_attributes = RenderableAttributeSet(
            MeshAttributes {},
            MaterialAttributes {
                .shader_definition = m_render_resource->GetShader()->GetCompiledShader()->GetDefinition(),
                .cull_faces = FaceCullMode::BACK }) });

    InitObject(m_view);

    m_render_resource->SetCameraResourceHandle(TResourceHandle<RenderCamera>(m_camera->GetRenderResource()));
    m_render_resource->SetSceneResourceHandle(TResourceHandle<RenderScene>(m_parent_scene->GetRenderResource()));
    m_render_resource->SetViewResourceHandle(TResourceHandle<RenderView>(m_view->GetRenderResource()));

    SetReady(true);
}

void EnvGrid::CreateEnvProbes()
{
    HYP_SCOPE;

    const uint32 num_ambient_probes = m_options.density.Volume();
    const Vec3f aabb_extent = m_aabb.GetExtent();

    const Vec2u probe_dimensions = GetProbeDimensions(m_options.type);
    AssertThrow(probe_dimensions.Volume() != 0);

    if (num_ambient_probes != 0)
    {
        for (uint32 x = 0; x < m_options.density.x; x++)
        {
            for (uint32 y = 0; y < m_options.density.y; y++)
            {
                for (uint32 z = 0; z < m_options.density.z; z++)
                {
                    const uint32 index = x * m_options.density.x * m_options.density.y
                        + y * m_options.density.x
                        + z;

                    const BoundingBox env_probe_aabb(
                        m_aabb.min + (Vec3f(float(x), float(y), float(z)) * SizeOfProbe()),
                        m_aabb.min + (Vec3f(float(x + 1), float(y + 1), float(z + 1)) * SizeOfProbe()));

                    Handle<EnvProbe> env_probe = CreateObject<EnvProbe>(
                        m_parent_scene,
                        env_probe_aabb,
                        probe_dimensions,
                        EnvProbeType::AMBIENT);

                    env_probe->m_grid_slot = index;

                    InitObject(env_probe);

                    m_env_probe_collection.AddProbe(index, env_probe);
                }
            }
        }
    }
}

void EnvGrid::SetAABB(const BoundingBox& aabb)
{
    HYP_SCOPE;

    if (m_aabb != aabb)
    {
        m_aabb = aabb;

        if (IsInitCalled())
        {
            m_render_resource->SetAABB(aabb);
        }
    }
}

void EnvGrid::SetParentScene(const Handle<Scene>& parent_scene)
{
    HYP_SCOPE;

    if (m_parent_scene == parent_scene)
    {
        return;
    }

    m_parent_scene = parent_scene;

    if (IsInitCalled())
    {
        if (parent_scene.IsValid())
        {
            AssertThrow(parent_scene->IsReady());

            m_render_resource->SetSceneResourceHandle(TResourceHandle<RenderScene>(parent_scene->GetRenderResource()));
        }
        else
        {
            m_render_resource->SetSceneResourceHandle(TResourceHandle<RenderScene>());
        }
    }
}

void EnvGrid::Translate(const BoundingBox& aabb, const Vec3f& translation)
{
    HYP_SCOPE;
    AssertReady();

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    m_aabb = aabb;

    const BoundingBox current_aabb = m_aabb;
    const Vec3f current_aabb_center = current_aabb.GetCenter();
    const Vec3f current_aabb_center_minus_offset = current_aabb_center - m_offset;

    const Vec3f size_of_probe = SizeOfProbe();

    const Vec3i position_snapped {
        MathUtil::Floor<float, Vec3i::Type>(translation.x / size_of_probe.x),
        MathUtil::Floor<float, Vec3i::Type>(translation.y / size_of_probe.y),
        MathUtil::Floor<float, Vec3i::Type>(translation.z / size_of_probe.z)
    };

    const Vec3i current_grid_position {
        MathUtil::Floor<float, Vec3i::Type>(current_aabb_center_minus_offset.x / size_of_probe.x),
        MathUtil::Floor<float, Vec3i::Type>(current_aabb_center_minus_offset.y / size_of_probe.y),
        MathUtil::Floor<float, Vec3i::Type>(current_aabb_center_minus_offset.z / size_of_probe.z)
    };

    m_aabb.SetCenter(Vec3f(position_snapped) * size_of_probe + m_offset);

    if (current_grid_position == position_snapped)
    {
        return;
    }

    if (m_camera)
    {
        m_camera->SetTranslation(m_aabb.GetCenter());
    }

    Array<uint32> updates;
    updates.Resize(m_env_probe_collection.num_probes);

    for (uint32 x = 0; x < m_options.density.x; x++)
    {
        for (uint32 y = 0; y < m_options.density.y; y++)
        {
            for (uint32 z = 0; z < m_options.density.z; z++)
            {
                const Vec3i coord { int(x), int(y), int(z) };
                const Vec3i scrolled_coord = coord + position_snapped;

                const Vec3i scrolled_coord_clamped {
                    MathUtil::Mod(scrolled_coord.x, int(m_options.density.x)),
                    MathUtil::Mod(scrolled_coord.y, int(m_options.density.y)),
                    MathUtil::Mod(scrolled_coord.z, int(m_options.density.z))
                };

                const int scrolled_clamped_index = scrolled_coord_clamped.x * m_options.density.x * m_options.density.y
                    + scrolled_coord_clamped.y * m_options.density.x
                    + scrolled_coord_clamped.z;

                const int index = x * m_options.density.x * m_options.density.y
                    + y * m_options.density.x
                    + z;

                AssertThrow(scrolled_clamped_index >= 0);

                const BoundingBox new_probe_aabb {
                    m_aabb.min + (Vec3f(float(x), float(y), float(z)) * size_of_probe),
                    m_aabb.min + (Vec3f(float(x + 1), float(y + 1), float(z + 1)) * size_of_probe)
                };

                const Handle<EnvProbe>& probe = m_env_probe_collection.GetEnvProbeDirect(scrolled_clamped_index);

                if (!probe.IsValid())
                {
                    // Should not happen, but just in case
                    continue;
                }

                // If probe is at the edge of the grid, it will be moved to the opposite edge.
                // That means we need to re-render
                if (!probe->GetAABB().ContainsPoint(new_probe_aabb.GetCenter()))
                {
                    probe->SetAABB(new_probe_aabb);
                }

                updates[index] = scrolled_clamped_index;
            }
        }
    }

    for (uint32 update_index = 0; update_index < uint32(updates.Size()); update_index++)
    {
        AssertThrow(update_index < m_env_probe_collection.num_probes);
        AssertThrow(updates[update_index] < m_env_probe_collection.num_probes);

        m_env_probe_collection.SetIndexOnGameThread(update_index, updates[update_index]);
    }

    m_render_resource->SetProbeIndices(std::move(updates));
}

void EnvGrid::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    HYP_LOG(EnvGrid, Debug, "Updating EnvGrid {} with AABB: {} on thread {}",
        GetID(), m_aabb, Threads::CurrentThreadID().GetName());

    m_camera->Update(delta);

    m_view->UpdateVisibility();
    m_view->Update(delta);

    for (uint32 index = 0; index < m_env_probe_collection.num_probes; index++)
    {
        const Handle<EnvProbe>& probe = m_env_probe_collection.GetEnvProbeDirect(index);
        AssertThrow(probe.IsValid());

        probe->SetNeedsUpdate(true);
        probe->SetNeedsRender(true);

        probe->Update(delta);
    }
}

#pragma endregion EnvGrid

} // namespace hyperion
