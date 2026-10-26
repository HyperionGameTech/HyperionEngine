/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>
#include <Animation/BoneAttachmentComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings, Serialize = false)
class BoneAttachmentSystem final : public SystemBase
{
    HYP_OBJECT_BODY(BoneAttachmentSystem);

public:
    ~BoneAttachmentSystem() override = default;

    bool ShouldProcessScene(Scene* scene) const override;

    bool AllowParallelExecution() const override
    {
        return false;
    }

    bool RequiresSimThread() const override
    {
        return true;
    }

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<BoneAttachmentComponent, ComponentAccess::READ> {},
            ComponentDescriptor<MeshComponent, ComponentAccess::READ, false> {}
        };
    }
};

} // namespace Hyperion
