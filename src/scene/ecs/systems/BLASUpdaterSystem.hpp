/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_BLAS_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_BLAS_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/EntityTag.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class BLASUpdaterSystem : public SystemBase
{
    HYP_OBJECT_BODY(BLASUpdaterSystem);

public:
    BLASUpdaterSystem(EntityManager& entity_manager);
    virtual ~BLASUpdaterSystem() override = default;

    virtual bool ShouldCreateForScene(Scene* scene) const override;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ> {},

            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_BLAS>, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }
};

} // namespace hyperion

#endif