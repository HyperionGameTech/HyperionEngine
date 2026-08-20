/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Components/PlayerComponent.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Containers/Map.hpp>

namespace Hyperion {

class CameraStreamingVolume;

/// Handles mapping entities tagged Player to a PlayerComponent on connect/disconnect.
/// Server+Client. Client handles its own connection, server handles all players connected to the server.
HYP_CLASS(NoScriptBindings)
class PlayerSystem final : public SystemBase
{
    HYP_OBJECT_BODY(PlayerSystem);

public:
    ~PlayerSystem() override = default;

    bool RequiresSimThread() const override
    {
        return true;
    }

    bool AllowUpdate() const override
    {
        return true;
    }

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<TagComponent<EntityTag::Player>, ComponentAccess::READ_WRITE, true> {},
            ComponentDescriptor<PlayerComponent, ComponentAccess::READ_WRITE, false> {}
        };
    }

private:
    void HandleClientConnected(net::NetConnectionId connectionId, bool isLocalPlayer);
    void HandleClientDisconnected(net::NetConnectionId connectionId, bool isLocalPlayer);

    bool TrySpawnPlayerEntity(net::NetConnectionId connectionId, bool isLocalPlayer);

    void UpdateStreamingVolumes();
    
    Map<UUID, Handle<Entity>, SceneAllocator> m_playerEntityTemplates;

    Map<net::NetConnectionId, Handle<Entity>, SceneAllocator> m_connectionIdToPlayerEntity;
    Map<net::NetConnectionId, Handle<CameraStreamingVolume>, SceneAllocator> m_connectionIdToStreamingVolume;

    // Pair is [id, isLocalPlayer]
    Array<Pair<net::NetConnectionId, bool>, SceneAllocator> m_pendingConnections;
};

} // namespace Hyperion
