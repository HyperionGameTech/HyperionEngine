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
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvGrid.hpp>

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

#pragma region EnvGridUpdaterSystem

EnvGridUpdaterSystem::EnvGridUpdaterSystem(EntityManager& entity_manager)
    : SystemBase(entity_manager)
{
}

void EnvGridUpdaterSystem::OnEntityAdded(const Handle<Entity>& entity)
{
    SystemBase::OnEntityAdded(entity);

    // EnvGridComponent& env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);
    // BoundingBoxComponent& bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    // if (env_grid_component.env_grid)
    // {
    //     return;
    // }

    // if (!GetWorld())
    // {
    //     return;
    // }

    // EnumFlags<EnvGridFlags> flags = EnvGridFlags::NONE;

    // if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.reflections.enabled").ToBool()
    //     || g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.mode").ToString().ToLower() == "voxel")
    // {
    //     flags |= EnvGridFlags::USE_VOXEL_GRID;
    // }

    // env_grid_component.env_grid = CreateObject<EnvGrid>(
    //     GetScene()->HandleFromThis(),
    //     bounding_box_component.world_aabb,
    //     EnvGridOptions {
    //         .type = env_grid_component.env_grid_type,
    //         .density = env_grid_component.grid_size,
    //         .flags = flags });

    // InitObject(env_grid_component.env_grid);
}

void EnvGridUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    // EnvGridComponent& env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);

    // env_grid_component.env_grid.Reset();

    // if (env_grid_component.env_grid_render_subsystem)
    // {
    //     env_grid_component.env_grid_render_subsystem->RemoveFromEnvironment();
    //     env_grid_component.env_grid_render_subsystem.Reset();
    // }
}

void EnvGridUpdaterSystem::Process(float delta)
{
    // { // Determine which EnvGrids needs to be updated on the game thread
    //     AfterProcess([this]()
    //         {
    //             for (auto [entity_id, _, bounding_box_component] : GetEntityManager().GetEntitySet<EntityType<EnvGrid>, BoundingBoxComponent>().GetScopedView(GetComponentInfos()))
    //             {
    //                 // @TODO Store pointer to Entity rather than ID so we don't need to lock the handle
    //                 Handle<Entity> entity { entity_id };
    //                 AssertDebug(entity->IsInstanceOf<EnvGrid>());

    //                 Handle<EnvGrid> env_grid = entity.Cast<EnvGrid>();

    //                 env_grid->SetReceivesUpdate(true);
    //             }
    //         });
    // }

    // { // Update camera data
    //     HashSet<ID<Entity>> updated_entity_ids;

    //     for (auto [entity_id, _t, transform_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<EntityType<EnvGrid>, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_ENV_GRID_TRANSFORM>>().GetScopedView(GetComponentInfos()))
    //     {
    //         if (!env_grid_component.env_grid)
    //         {
    //             continue;
    //         }

    //         env_grid_component.env_grid->Translate(bounding_box_component.world_aabb, transform_component.transform.GetTranslation());

    //         updated_entity_ids.Insert(entity_id);
    //     }

    //     if (updated_entity_ids.Any())
    //     {
    //         AfterProcess([this, updated_entity_ids = std::move(updated_entity_ids)]()
    //             {
    //                 for (const ID<Entity>& entity_id : updated_entity_ids)
    //                 {
    //                     GetEntityManager().RemoveTag<EntityTag::UPDATE_ENV_GRID_TRANSFORM>(entity_id);
    //                 }
    //             });
    //     }
    // }

    // { // Update EnvGrid transforms to map to the camera
    //     HashSet<ID<Entity>> updated_entity_ids;

    //     for (auto [entity_id, env_grid_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<EnvGridComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos()))
    //     {
    //         if (!env_grid_component.env_grid)
    //         {
    //             continue;
    //         }

    //         // Update movable envgrids
    //         if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA)
    //         {
    //             const Handle<Camera>& camera = GetScene()->GetPrimaryCamera();

    //             if (camera.IsValid())
    //             {
    //                 Vec3f translation = transform_component.transform.GetTranslation();

    //                 if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_X)
    //                 {
    //                     translation.x = camera->GetTranslation().x;
    //                 }

    //                 if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_Y)
    //                 {
    //                     translation.y = camera->GetTranslation().y;
    //                 }

    //                 if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_Z)
    //                 {
    //                     translation.z = camera->GetTranslation().z;
    //                 }

    //                 if (translation != transform_component.transform.GetTranslation())
    //                 {
    //                     transform_component.transform.SetTranslation(translation);

    //                     updated_entity_ids.Insert(entity_id);
    //                 }
    //             }
    //         }
    //     }

    //     if (updated_entity_ids.Any())
    //     {
    //         AfterProcess([this, updated_entity_ids = std::move(updated_entity_ids)]()
    //             {
    //                 for (const ID<Entity>& entity_id : updated_entity_ids)
    //                 {
    //                     GetEntityManager().AddTag<EntityTag::UPDATE_ENV_GRID_TRANSFORM>(entity_id);
    //                 }
    //             });
    //     }
    // }
}

#pragma endregion EnvGridUpdaterSystem

} // namespace hyperion
