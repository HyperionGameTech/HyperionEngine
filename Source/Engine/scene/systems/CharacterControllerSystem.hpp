/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <scene/System.hpp>
#include <scene/components/CharacterControllerComponent.hpp>
#include <scene/components/TransformComponent.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class CharacterControllerSystem : public SystemBase
{
    HYP_OBJECT_BODY(CharacterControllerSystem);

public:
    virtual ~CharacterControllerSystem() override = default;

    virtual bool ShouldProcessScene(Scene* scene) const override;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

    virtual bool RequiresSimThread() const override { return true; }
    virtual bool AllowParallelExecution() const override { return false; }

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<CharacterControllerComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ_WRITE> {}
        };
    }
};

} // namespace Hyperion
