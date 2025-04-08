/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SCENE_PRIMARY_CAMERA_SYSTEM_HPP
#define HYPERION_ECS_SCENE_PRIMARY_CAMERA_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/CameraComponent.hpp>

#include <scene/ecs/EntityTag.hpp>

namespace hyperion {

class ScenePrimaryCameraSystem final : public System<
    ScenePrimaryCameraSystem,
    ComponentDescriptor<CameraComponent, COMPONENT_RW_FLAGS_READ>,
    ComponentDescriptor<EntityTagComponent<EntityTag::CAMERA_PRIMARY>, COMPONENT_RW_FLAGS_READ>
>
{
public:
    ScenePrimaryCameraSystem(EntityManager &entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~ScenePrimaryCameraSystem() override = default;

    virtual bool AllowUpdate() const override
        { return false; }

    virtual EnumFlags<SceneFlags> GetRequiredSceneFlags() const override;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif