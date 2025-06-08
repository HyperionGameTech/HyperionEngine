/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Node.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderState.hpp>

#include <rendering/backend/RendererShader.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypEnum.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <system/AppContext.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvGrid);

#pragma region EnvGridRenderSubsystem

class EnvGridRenderSubsystem : public RenderSubsystem
{
public:
    EnvGridRenderSubsystem(Name name, const TResourceHandle<RenderEnvGrid>& env_render_grid)
        : RenderSubsystem(name),
          m_env_render_grid(env_render_grid)
    {
    }

    virtual ~EnvGridRenderSubsystem() override = default;

    virtual void Init() override
    {
        // @TODO refactor
        g_engine->GetRenderState()->BindEnvGrid(TResourceHandle<RenderEnvGrid>(m_env_render_grid));
    }

    virtual void OnRemoved() override
    {
        g_engine->GetRenderState()->UnbindEnvGrid();
    }

    virtual void OnRender(FrameBase* frame, const RenderSetup& render_setup) override
    {
        m_env_render_grid->Render(frame, render_setup);
    }

private:
    TResourceHandle<RenderEnvGrid> m_env_render_grid;
};

#pragma endregion EnvGridRenderSubsystem

#pragma region EnvGridUpdaterSystem

EnvGridUpdaterSystem::EnvGridUpdaterSystem(EntityManager& entity_manager)
    : System(entity_manager)
{
}

void EnvGridUpdaterSystem::OnEntityAdded(const Handle<Entity>& entity)
{
    SystemBase::OnEntityAdded(entity);

    EnvGridComponent& env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);
    BoundingBoxComponent& bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    if (env_grid_component.env_grid)
    {
        return;
    }

    if (!GetWorld())
    {
        return;
    }

    if (GetScene()->IsForegroundScene())
    {
        EnumFlags<EnvGridFlags> flags = EnvGridFlags::NONE;

        if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.reflections.enabled").ToBool()
            || g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.mode").ToString().ToLower() == "voxel")
        {
            flags |= EnvGridFlags::USE_VOXEL_GRID;
        }

        env_grid_component.env_grid = CreateObject<EnvGrid>(
            GetScene()->HandleFromThis(),
            bounding_box_component.world_aabb,
            EnvGridOptions {
                .type = env_grid_component.env_grid_type,
                .density = env_grid_component.grid_size,
                .flags = flags });

        InitObject(env_grid_component.env_grid);

        env_grid_component.env_render_grid = TResourceHandle<RenderEnvGrid>(env_grid_component.env_grid->GetRenderResource());
        env_grid_component.env_grid_render_subsystem = GetScene()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<EnvGridRenderSubsystem>(
            Name::Unique("EnvGridRenderSubsystem"),
            env_grid_component.env_render_grid);
    }
}

void EnvGridUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    EnvGridComponent& env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);

    env_grid_component.env_render_grid.Reset();

    if (env_grid_component.env_grid_render_subsystem)
    {
        env_grid_component.env_grid_render_subsystem->RemoveFromEnvironment();
        env_grid_component.env_grid_render_subsystem.Reset();
    }
}

void EnvGridUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    { // Determine which EnvGrids needs to be updated on the game thread
        AfterProcess([this]()
            {
                for (auto [entity_id, env_grid_component, bounding_box_component] : GetEntityManager().GetEntitySet<EnvGridComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos()))
                {
                    // Skip if the entity is already marked for update
                    if (GetEntityManager().HasTag<EntityTag::UPDATE_ENV_GRID>(entity_id))
                    {
                        continue;
                    }

                    if (!env_grid_component.env_grid)
                    {
                        continue;
                    }

                    bool should_recollect_entites = false;

                    if (!env_grid_component.env_grid->GetCamera()->IsReady())
                    {
                        should_recollect_entites = true;
                    }

                    Octree const* octree = &GetScene()->GetOctree();
                    octree->GetFittingOctant(bounding_box_component.world_aabb, octree);

                    const HashCode octant_hash_code = octree->GetOctantID().GetHashCode().Add(octree->GetEntryListHash<EntityTag::STATIC>()).Add(octree->GetEntryListHash<EntityTag::LIGHT>());

                    if (octant_hash_code != env_grid_component.octant_hash_code)
                    {
                        HYP_LOG(EnvGrid, Debug, "EnvGrid octant hash code changed ({} != {}), updating probes", env_grid_component.octant_hash_code.Value(), octant_hash_code.Value());

                        env_grid_component.octant_hash_code = octant_hash_code;

                        should_recollect_entites = true;
                    }

                    if (!should_recollect_entites)
                    {
                        continue;
                    }

                    GetEntityManager().AddTag<EntityTag::UPDATE_ENV_GRID>(entity_id);
                }
            });
    }

    { // Update camera data
        HashSet<ID<Entity>> updated_entity_ids;

        for (auto [entity_id, env_grid_component, transform_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<EnvGridComponent, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_ENV_GRID_TRANSFORM>>().GetScopedView(GetComponentInfos()))
        {
            if (!env_grid_component.env_grid)
            {
                continue;
            }

            env_grid_component.env_grid->Translate(bounding_box_component.world_aabb, transform_component.transform.GetTranslation());

            updated_entity_ids.Insert(entity_id);
        }

        if (updated_entity_ids.Any())
        {
            AfterProcess([this, updated_entity_ids = std::move(updated_entity_ids)]()
                {
                    for (const ID<Entity>& entity_id : updated_entity_ids)
                    {
                        GetEntityManager().RemoveTag<EntityTag::UPDATE_ENV_GRID_TRANSFORM>(entity_id);
                    }
                });
        }
    }

    { // Update EnvGrid transforms to map to the camera
        HashSet<ID<Entity>> updated_entity_ids;

        for (auto [entity_id, env_grid_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<EnvGridComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!env_grid_component.env_grid)
            {
                continue;
            }

            // Update movable envgrids
            if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA)
            {
                const Handle<Camera>& camera = GetScene()->GetPrimaryCamera();

                if (camera.IsValid())
                {
                    Vec3f translation = transform_component.transform.GetTranslation();

                    if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_X)
                    {
                        translation.x = camera->GetTranslation().x;
                    }

                    if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_Y)
                    {
                        translation.y = camera->GetTranslation().y;
                    }

                    if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_Z)
                    {
                        translation.z = camera->GetTranslation().z;
                    }

                    if (translation != transform_component.transform.GetTranslation())
                    {
                        transform_component.transform.SetTranslation(translation);

                        updated_entity_ids.Insert(entity_id);
                    }
                }
            }
        }

        if (updated_entity_ids.Any())
        {
            AfterProcess([this, updated_entity_ids = std::move(updated_entity_ids)]()
                {
                    for (const ID<Entity>& entity_id : updated_entity_ids)
                    {
                        GetEntityManager().AddTag<EntityTag::UPDATE_ENV_GRID_TRANSFORM>(entity_id);
                    }
                });
        }
    }

    { // Process each EnvGrid that has the UPDATE_ENV_GRID tag
        HashSet<ID<Entity>> updated_entity_ids;

        for (auto [entity_id, env_grid_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<EnvGridComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_ENV_GRID>>().GetScopedView(GetComponentInfos()))
        {
            if (!env_grid_component.env_grid)
            {
                continue;
            }

            if (!env_grid_component.env_grid->GetCamera()->IsReady())
            {
                continue;
            }

            UpdateEnvGrid(delta, env_grid_component, bounding_box_component);

            updated_entity_ids.Insert(entity_id);
        }

        if (updated_entity_ids.Any())
        {
            AfterProcess([this, updated_entity_ids = std::move(updated_entity_ids)]()
                {
                    for (const ID<Entity>& entity_id : updated_entity_ids)
                    {
                        GetEntityManager().RemoveTag<EntityTag::UPDATE_ENV_GRID>(entity_id);
                    }
                });
        }
    }
}

void EnvGridUpdaterSystem::UpdateEnvGrid(GameCounter::TickUnit delta, EnvGridComponent& env_grid_component, BoundingBoxComponent& bounding_box_component)
{
    HYP_SCOPE;

    env_grid_component.env_grid->Update(delta);
}

#pragma endregion EnvGridUpdaterSystem

} // namespace hyperion
