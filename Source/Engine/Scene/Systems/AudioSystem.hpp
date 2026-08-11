/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/Components/AudioComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings, Serialize=false)
class AudioSystem final : public SystemBase
{
    HYP_OBJECT_BODY(AudioSystem);

public:
    ~AudioSystem() override = default;

    bool ShouldProcessScene(Scene* scene) const override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<AudioComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ> {}
        };
    }
};

} // namespace Hyperion
