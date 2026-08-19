/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Components/ReplicationStateComponent.hpp>

#include <Core/Logging/LoggerFwd.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Replication);

HYP_CLASS(NoScriptBindings)
class ReplicationSystem final : public SystemBase
{
    HYP_OBJECT_BODY(ReplicationSystem);

public:
    ~ReplicationSystem() override = default;

    bool RequiresSimThread() const override
    {
        return true;
    }

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<TagComponent<EntityTag::Replicated>, ComponentAccess::READ, true> {},
            ComponentDescriptor<TagComponent<EntityTag::UpdateReplication>, ComponentAccess::READ, false> {},
            
            ComponentDescriptor<ReplicationStateComponent, ComponentAccess::READ_WRITE, false> {}
        };
    }

}; // class PhysicsSystem

} // namespace Hyperion
