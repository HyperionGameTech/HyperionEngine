/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_BVH_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_BVH_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/BVHComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

namespace hyperion {

class BVHUpdaterSystem : public System<
    BVHUpdaterSystem,
    ComponentDescriptor<BVHComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,

    ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_BVH>, COMPONENT_RW_FLAGS_READ, false>
>
{
public:
    BVHUpdaterSystem(EntityManager &entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~BVHUpdaterSystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif