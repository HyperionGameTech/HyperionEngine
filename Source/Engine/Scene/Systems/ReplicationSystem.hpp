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

#include <Framework/Net/PlayerMove.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/Set.hpp>

#include <Core/Math/Vector3.hpp>

#include <Core/Utilities/Tuple.hpp>

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
    // Pending player moves received from a client, to be consumed in order.
    struct PlayerMoveQueueState
    {
        static constexpr uint32 MaxQueuedMoves = 128;

        // Seconds of move time a connection can bank. Refilled with real server time, spent by applied moves.
        static constexpr float MaxTimeBudget = 0.5f;

        Array<PlayerMove, SceneAllocator> moves;
        uint32 lastQueuedMoveId = 0;
        float timeBudget = MaxTimeBudget;
    };

    void ApplyPendingRequests();
    void ProcessPlayerMoves(float delta);
    void ProcessInterestSpawns(Span<const Handle<Scene>> scenes, const Array<Tuple<net::NetConnectionId, Vec3f>, SceneTempAllocator>& playerPositions);

    Map<NetId, Handle<Entity>, SceneAllocator> m_netIdToEntity;
    Map<net::NetConnectionId, Entity*, SceneAllocator> m_connectionIdToEntity;
    Map<net::NetConnectionId, PlayerMoveQueueState, SceneAllocator> m_playerMoveQueues;

    // NetIds each connection has been sent an EntitySpawn for. They stay known after leaving range, since a
    // despawn would make the client Remove() level entities it resolved by UUID.
    Map<net::NetConnectionId, Set<NetId, SceneAllocator>, SceneAllocator> m_knownEntities;
}; // class ReplicationSystem

} // namespace Hyperion
