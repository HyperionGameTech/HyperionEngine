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

HYP_CLASS(NoScriptBindings, Authoritative)
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
        return EngineGlobals::IsServer();
    }

    void OnEntityAdded(Entity* entity) override;

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<TagComponent<EntityTag::Player>, ComponentAccess::READ_WRITE, true> {},
            ComponentDescriptor<PlayerComponent, ComponentAccess::READ_WRITE, false> {},
            ComponentDescriptor<TagComponent<EntityTag::Replicated>, ComponentAccess::READ_WRITE, false> {}
        };
    }

private:
    void HandleClientConnected(net::NetConnectionId connectionId);
    void HandleClientDisconnected(net::NetConnectionId connectionId);
    bool TrySpawnClone(net::NetConnectionId connectionId);

    void UpdateStreamingVolumes();

    Handle<Entity> m_templateEntity;
    Map<net::NetConnectionId, Handle<Entity>> m_connectionIdToClone;
    Map<net::NetConnectionId, Handle<CameraStreamingVolume>> m_connectionIdToStreamingVolume;
    Array<net::NetConnectionId> m_pendingConnections;
};

} // namespace Hyperion
