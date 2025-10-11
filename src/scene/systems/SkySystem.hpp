/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/EntityTag.hpp>
#include <scene/components/SkyComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/MeshComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class SkySystem : public SystemBase
{
    HYP_OBJECT_BODY(SkySystem);

public:
    SkySystem(EntityManager& entityManager);
    virtual ~SkySystem() override = default;

    virtual bool RequiresGameThread() const override
    {
        return true;
    }

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<SkyComponent, ComponentRWFlags::READ_WRITE> {},

            // calling EnvProbe::Update() calls View::Update() which reads the following components on entities it processes.
            ComponentDescriptor<MeshComponent, ComponentRWFlags::READ, false> {},
            ComponentDescriptor<TransformComponent, ComponentRWFlags::READ, false> {},
            ComponentDescriptor<BoundingBoxComponent, ComponentRWFlags::READ, false> {},

            ComponentDescriptor<TagComponent<EntityTag::STATIC>, ComponentRWFlags::READ, false> {}
        };
    }

    void AddRenderSubsystemToEnvironment(World* world, EntityManager& mgr, Entity* entity, SkyComponent& skyComponent, MeshComponent* meshComponent);
};

} // namespace hyperion

