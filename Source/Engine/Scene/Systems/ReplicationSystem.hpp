/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Components/ReplicationStateComponent.hpp>
#include <Scene/Components/PlayerComponent.hpp>

#include <Core/Containers/Map.hpp>

namespace Hyperion {

enum class NetId : uint32;

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

            ComponentDescriptor<ReplicationStateComponent, ComponentAccess::READ_WRITE, false> {},
            ComponentDescriptor<PlayerComponent, ComponentAccess::READ, false> {}
        };
    }

private:
    void ApplyPendingRequests();
    void ProcessPendingCatchUp(Span<Handle<Scene>> scenes);

    Map<NetId, Handle<Entity>, SceneAllocator> m_netIdToEntity;
    Array<net::NetConnectionId> m_pendingCatchUpConnections;
}; // class PhysicsSystem

} // namespace Hyperion
