/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/EntityTag.hpp>

#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings, Serialize=false)
class VisibilityStateUpdaterSystem : public SystemBase
{
    HYP_OBJECT_BODY(VisibilityStateUpdaterSystem);

public:
    virtual ~VisibilityStateUpdaterSystem() override = default;

    virtual bool ShouldProcessScene(Scene* scene) const override;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<VisibilityStateComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<BoundingBoxComponent, ComponentAccess::READ> {},

            ComponentDescriptor<TagComponent<EntityTag::UpdateVisibility>, ComponentAccess::READ, false> {}
        };
    }
};

} // namespace Hyperion
