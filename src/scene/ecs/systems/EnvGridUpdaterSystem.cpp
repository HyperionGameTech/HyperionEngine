/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Node.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>

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

EnvGridUpdaterSystem::EnvGridUpdaterSystem(EntityManager &entity_manager)
    : System(entity_manager)
{
    m_delegate_handlers.Add(NAME("OnWorldChange"), OnWorldChanged.Bind([this](World *new_world, World *previous_world)
    {
        Threads::AssertOnThread(g_game_thread);

        // Trigger removal and addition of render subsystems
        for (auto [entity_id, env_grid_component, bounding_box_component] : GetEntityManager().GetEntitySet<EnvGridComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos())) {
            if (env_grid_component.env_grid) {
                env_grid_component.env_grid->RemoveFromEnvironment();
            }

            if (!new_world) {
                env_grid_component.env_grid.Reset();
            }

            if (GetScene()->IsForegroundScene()) {
                AddRenderSubsystemToEnvironment(env_grid_component, bounding_box_component, new_world);
            }
        }
    }));
}

void EnvGridUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    EnvGridComponent &env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);
    BoundingBoxComponent &bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    if (env_grid_component.env_grid) {
        return;
    }

    if (!GetWorld()) {
        return;
    }

    if (GetScene()->IsForegroundScene()) {
        AddRenderSubsystemToEnvironment(env_grid_component, bounding_box_component, GetWorld());
    }
}

void EnvGridUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    EnvGridComponent &env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);

    if (env_grid_component.env_grid) {
        env_grid_component.env_grid->RemoveFromEnvironment();
        env_grid_component.env_grid.Reset();
    }
}

void EnvGridUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    { // Determine which EnvGrids needs to be updated on the game thread
        AfterProcess([this]()
        {
            for (auto [entity_id, env_grid_component, bounding_box_component] : GetEntityManager().GetEntitySet<EnvGridComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos())) {
                // Skip if the entity is already marked for update
                if (GetEntityManager().HasTag<EntityTag::UPDATE_ENV_GRID>(entity_id)) {
                    continue;
                }
                
                if (!env_grid_component.env_grid) {
                    continue;
                }

                bool should_recollect_entites = false;

                if (!env_grid_component.env_grid->GetCamera()->IsReady()) {
                    should_recollect_entites = true;
                }

                Octree const *octree = &GetScene()->GetOctree();
                octree->GetFittingOctant(bounding_box_component.world_aabb, octree);
                
                const HashCode octant_hash_code = octree->GetOctantID().GetHashCode()
                    .Add(octree->GetEntryListHash<EntityTag::STATIC>())
                    .Add(octree->GetEntryListHash<EntityTag::LIGHT>());

                if (octant_hash_code != env_grid_component.octant_hash_code) {
                    HYP_LOG(EnvGrid, Debug, "EnvGrid octant hash code changed ({} != {}), updating probes", env_grid_component.octant_hash_code.Value(), octant_hash_code.Value());

                    env_grid_component.octant_hash_code = octant_hash_code;

                    should_recollect_entites = true;
                }

                if (!should_recollect_entites) {
                    continue;
                }

                GetEntityManager().AddTag<EntityTag::UPDATE_ENV_GRID>(entity_id);
            }
        });
    }

    { // Update camera data
        HashSet<ID<Entity>> updated_entity_ids;

        for (auto [entity_id, env_grid_component, transform_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<EnvGridComponent, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_ENV_GRID_TRANSFORM>>().GetScopedView(GetComponentInfos())) {
            if (!env_grid_component.env_grid) {
                continue;
            }
            env_grid_component.env_grid->SetCameraData(bounding_box_component.world_aabb, transform_component.transform.GetTranslation());

            updated_entity_ids.Insert(entity_id);
        }

        if (updated_entity_ids.Any()) {
            AfterProcess([this, updated_entity_ids = std::move(updated_entity_ids)]()
            {
                for (const ID<Entity> &entity_id : updated_entity_ids) {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_ENV_GRID_TRANSFORM>(entity_id);
                }
            });
        }
    }

    { // Update EnvGrid transforms to map to the camera
        for (auto [entity_id, env_grid_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<EnvGridComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos())) {
            if (!env_grid_component.env_grid) {
                continue;
            }

            // Update movable envgrids
            if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA) {
                const Handle<Camera> &camera = GetScene()->GetCamera();

                if (camera.IsValid()) {
                    Vec3f translation = transform_component.transform.GetTranslation();

                    if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_X) {
                        translation.x = camera->GetTranslation().x;
                    }

                    if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_Y) {
                        translation.y = camera->GetTranslation().y;
                    }

                    if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_Z) {
                        translation.z = camera->GetTranslation().z;
                    }

                    if (translation != transform_component.transform.GetTranslation()) {
                        // @TODO If attached to a Node it should update the Node's world transform
                        transform_component.transform.SetTranslation(translation);
                    }
                }
            }
        }
    }

    { // Process each EnvGrid that has the UPDATE_ENV_GRID tag
        HashSet<ID<Entity>> updated_entity_ids;

        for (auto [entity_id, env_grid_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<EnvGridComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_ENV_GRID>>().GetScopedView(GetComponentInfos())) {
            if (!env_grid_component.env_grid) {
                continue;
            }

            UpdateEnvGrid(delta, env_grid_component, bounding_box_component);

            updated_entity_ids.Insert(entity_id);
        }

        if (updated_entity_ids.Any()) {
            AfterProcess([this, updated_entity_ids = std::move(updated_entity_ids)]()
            {
                for (const ID<Entity> &entity_id : updated_entity_ids) {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_ENV_GRID>(entity_id);
                }
            });
        }
    }
}

void EnvGridUpdaterSystem::UpdateEnvGrid(GameCounter::TickUnit delta, EnvGridComponent &env_grid_component, BoundingBoxComponent &bounding_box_component)
{
    HYP_SCOPE;

    env_grid_component.env_grid->GetCamera()->Update(delta);

    GetScene()->CollectStaticEntities(
        env_grid_component.env_grid->GetRenderCollector(),
        env_grid_component.env_grid->GetCamera(),
        RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .shader_definition  = env_grid_component.env_grid->GetShader()->GetCompiledShader()->GetDefinition(),
                .cull_faces         = FaceCullMode::BACK
            }
        ),
        true // skip frustum culling, until Camera supports multiple frustums.
    );

    for (uint32 index = 0; index < env_grid_component.env_grid->GetEnvProbeCollection().num_probes; index++) {
        // Don't worry about using the indirect index
        const Handle<EnvProbe> &probe = env_grid_component.env_grid->GetEnvProbeCollection().GetEnvProbeDirect(index);
        AssertThrow(probe.IsValid());

        probe->SetNeedsUpdate(true);
        probe->Update(delta);
    }
}

void EnvGridUpdaterSystem::AddRenderSubsystemToEnvironment(EnvGridComponent &env_grid_component, BoundingBoxComponent &bounding_box_component, World *world)
{
    AssertThrow(world != nullptr);
    AssertThrow(world->IsReady());

    if (env_grid_component.env_grid) {
        world->GetRenderResource().GetEnvironment()->AddRenderSubsystem(env_grid_component.env_grid);
    } else {
        EnumFlags<EnvGridFlags> flags = EnvGridFlags::NONE;

        if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.reflections.enabled").ToBool()
            || g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.mode").ToString().ToLower() == "voxel")
        {
            flags |= EnvGridFlags::USE_VOXEL_GRID;
        }
        
        BoundingBox world_aabb = bounding_box_component.world_aabb;

        if (!world_aabb.IsFinite()) {
            world_aabb = BoundingBox::Empty();
        }

        if (!world_aabb.IsValid()) {
            return;
        }

        env_grid_component.env_grid = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<EnvGrid>(
            Name::Unique("env_grid_renderer"),
            GetScene()->HandleFromThis(),
            EnvGridOptions {
                env_grid_component.env_grid_type,
                world_aabb,
                env_grid_component.grid_size,
                flags
            }
        );
    }
}

} // namespace hyperion
