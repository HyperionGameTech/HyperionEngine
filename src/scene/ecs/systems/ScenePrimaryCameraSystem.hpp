/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SCENE_PRIMARY_CAMERA_SYSTEM_HPP
#define HYPERION_ECS_SCENE_PRIMARY_CAMERA_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/CameraComponent.hpp>

#include <scene/ecs/EntityTag.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class ScenePrimaryCameraSystem final : public SystemBase
{
    HYP_OBJECT_BODY(ScenePrimaryCameraSystem);

public:
    ScenePrimaryCameraSystem(EntityManager& entity_manager)
        : SystemBase(entity_manager)
    {
    }

    virtual ~ScenePrimaryCameraSystem() override = default;

    virtual bool AllowUpdate() const override
    {
        return false;
    }

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<CameraComponent, COMPONENT_RW_FLAGS_READ> {},
            ComponentDescriptor<EntityTagComponent<EntityTag::CAMERA_PRIMARY>, COMPONENT_RW_FLAGS_READ> {}
        };
    }
};

} // namespace hyperion

#endif