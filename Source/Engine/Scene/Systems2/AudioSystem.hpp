/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/components/AudioComponent.hpp>
#include <Scene/components/TransformComponent.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings, Serialize=false)
class AudioSystem : public SystemBase
{
    HYP_OBJECT_BODY(AudioSystem);

public:
    virtual ~AudioSystem() override = default;

    virtual bool ShouldProcessScene(Scene* scene) const override;

    virtual void OnEntityAdded(Entity* entity) override;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<AudioComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ> {}
        };
    }
};

} // namespace Hyperion
