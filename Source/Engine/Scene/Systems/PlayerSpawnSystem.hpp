/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Components/PlayerComponent.hpp>

#include <Core/Containers/Map.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings, Authoritative)
class PlayerSpawnSystem final : public SystemBase
{
    HYP_OBJECT_BODY(PlayerSpawnSystem);

public:
    ~PlayerSpawnSystem() override = default;

    bool RequiresSimThread() const override
    {
        return true;
    }

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<TagComponent<EntityTag::Player>, ComponentAccess::READ_WRITE, false> {},
            ComponentDescriptor<PlayerComponent, ComponentAccess::READ_WRITE, false> {},
            ComponentDescriptor<TagComponent<EntityTag::Replicated>, ComponentAccess::READ_WRITE, false> {}
        };
    }

private:
    void ResolveTemplate(Span<Handle<Scene>> scenes);
    void HandleClientConnected(net::NetConnectionId connectionId);
    void HandleClientDisconnected(net::NetConnectionId connectionId);
    bool TrySpawnClone(net::NetConnectionId connectionId);

    Handle<Entity> m_templateEntity;
    Map<net::NetConnectionId, Handle<Entity>> m_connectionIdToClone;
    Array<net::NetConnectionId> m_pendingConnections;
};

} // namespace Hyperion
