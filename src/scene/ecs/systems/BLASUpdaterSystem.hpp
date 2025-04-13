/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_BLAS_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_BLAS_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/EntityTag.hpp>

namespace hyperion {

class BLASUpdaterSystem : public System<
    BLASUpdaterSystem,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,

    ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_BLAS>, COMPONENT_RW_FLAGS_READ, false>
>
{
public:
    BLASUpdaterSystem(EntityManager &entity_manager);
    virtual ~BLASUpdaterSystem() override = default;
    
    virtual EnumFlags<SceneFlags> GetRequiredSceneFlags() const override;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif