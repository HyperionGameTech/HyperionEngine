/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>

#include <Scene/Components/CharacterModelComponent.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>
#include <Scene/Components/AnimationComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/PlayerComponent.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings, Serialize = false)
class CharacterModelSystem final : public SystemBase
{
    HYP_OBJECT_BODY(CharacterModelSystem);

public:
    ~CharacterModelSystem() override = default;

    bool ShouldProcessScene(Scene* scene) const override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    bool RequiresSimThread() const override { return true; }
    bool AllowParallelExecution() const override { return false; }

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<CharacterModelComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<CharacterControllerComponent, ComponentAccess::READ, false> {},
            ComponentDescriptor<AnimationComponent, ComponentAccess::READ_WRITE, false> {},
            ComponentDescriptor<MeshComponent, ComponentAccess::READ, false> {},
            ComponentDescriptor<PlayerComponent, ComponentAccess::READ, false> {}
        };
    }
};

} // namespace Hyperion
