/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Components/PlayerComponent.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class PlayerComponentSystem final : public SystemBase
{
    HYP_OBJECT_BODY(PlayerComponentSystem);

public:
    ~PlayerComponentSystem() override = default;

    bool RequiresSimThread() const override
    {
        return true;
    }

    virtual bool AllowUpdate() const
    {
        return false;
    }

    void OnEntityAdded(Entity* entity) override;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<TagComponent<EntityTag::Player>, ComponentAccess::READ, true> {},
            ComponentDescriptor<PlayerComponent, ComponentAccess::READ_WRITE, false> {}
        };
    }
};

} // namespace Hyperion
