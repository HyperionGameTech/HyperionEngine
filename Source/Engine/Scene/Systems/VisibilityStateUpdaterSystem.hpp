/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings, Serialize = false)
class VisibilityStateUpdaterSystem final : public SystemBase
{
    HYP_OBJECT_BODY(VisibilityStateUpdaterSystem);

public:
    ~VisibilityStateUpdaterSystem() override = default;

    bool AllowUpdate() const override
    {
        return false;
    }

    bool ShouldProcessScene(Scene* scene) const override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override
    {
        // No-op
    }

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<VisibilityStateComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<BoundingBoxComponent, ComponentAccess::READ> {},

            ComponentDescriptor<TagComponent<EntityTag::UpdateVisibility>, ComponentAccess::READ, false> {}
        };
    }
};

} // namespace Hyperion
