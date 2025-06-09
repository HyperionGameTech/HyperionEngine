/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_CAMERA_SYSTEM_HPP
#define HYPERION_ECS_CAMERA_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/CameraComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <scene/ecs/EntityTag.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class CameraSystem final : public SystemBase
{
    HYP_OBJECT_BODY(CameraSystem);

public:
    CameraSystem(EntityManager& entity_manager)
        : SystemBase(entity_manager)
    {
    }

    virtual ~CameraSystem() override = default;

    virtual bool RequiresGameThread() const override
    {
        return true;
    }

    virtual void OnEntityAdded(const Handle<Entity>& entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<CameraComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},

            ComponentDescriptor<NodeLinkComponent, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_CAMERA_TRANSFORM>, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }
};

} // namespace hyperion

#endif