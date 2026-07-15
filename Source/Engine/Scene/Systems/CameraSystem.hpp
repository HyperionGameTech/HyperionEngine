/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>

namespace Hyperion {

class Camera;

HYP_CLASS(NoScriptBindings)
class CameraSystem final : public SystemBase
{
    HYP_OBJECT_BODY(CameraSystem);

public:
    ~CameraSystem() override = default;

    bool ShouldProcessScene(Scene* scene) const override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    bool RequiresSimThread() const override { return true; }
    bool AllowParallelExecution() const override { return false; }

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<EntityType<Camera>, ComponentAccess::READ_WRITE> {}
        };
    }
};

} // namespace Hyperion
