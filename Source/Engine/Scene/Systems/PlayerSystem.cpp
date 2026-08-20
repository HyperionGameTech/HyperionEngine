/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/PlayerSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/Streaming/CameraStreamingVolume.hpp>

#include <Scene/Components/ReplicationStateComponent.hpp>

#include <Framework/Server/GameServer.hpp>
#include <Framework/Client/GameClient.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Net/NetServer.hpp>

#include <Streaming/StreamingManager.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>

#include <Core/Logging/Logger.hpp>

#include <PlayerSystem.generated.inl>

namespace Hyperion {

void PlayerSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (entity->HasComponent<PlayerComponent>())
    {
        PlayerComponent playerComponent = entity->GetComponent<PlayerComponent>();

        if (playerComponent.connectionId != Invalid<net::NetConnectionId>)
        {
            // Associated with a player; not a template!
            return;
        }

        // Remove so we don't add it to the Clones() and they can add their own with the correct connection ID.
        // PlayerComponent is added when client connects.
        entity->RemoveComponent<PlayerComponent>();
    }

    const UUID& uuid = entity->GetUUID();
    if (m_playerEntityTemplates.Contains(uuid))
    {
        return;
    }

    Handle<Entity> templateEntity = DynamicCast<Entity>(entity->Clone());
    Assert(templateEntity.IsValid());

    m_playerEntityTemplates[uuid] = templateEntity;
}

void PlayerSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
    
    // When the `Player` EntityTag is removed, remove the actual component too

    if (entity->HasComponent<PlayerComponent>())
    {
        // @TODO Disconnect/unregister?
        // Tag only likely to be removed in-editor.

        entity->RemoveComponent<PlayerComponent>();
    }
}

void PlayerSystem::OnAddedToWorld(World* world)
{
    SystemBase::OnAddedToWorld(world);

    // Client delegates first
    if (g_gameClient != nullptr)
    {
        m_delegateHandlers.Add(
            NAME("OnConnected"),
            g_gameClient->OnConnected.BindThreaded(
                [this](net::NetConnectionId id)
                {
                    HandleClientConnected(id);
                }, g_simThread));
        
        m_delegateHandlers.Add(
            NAME("OnDisconnected"),
            g_gameClient->OnDisconnected.BindThreaded(
                [this](net::NetConnectionId id)
                {
                    HandleClientDisconnected(id);
                }, g_simThread));
    }

    if (!EngineGlobals::IsServer())
    {
        return;
    }

    Assert(g_gameServer != nullptr);

    m_delegateHandlers.Add(
        NAME("OnClientConnected"),
        g_gameServer->GetNetServer().OnClientConnected.BindThreaded(
            [this](const NetServerConnectionStateChangedData& data)
            {
                HandleClientConnected(data.connectionId);
            }, g_simThread));

    m_delegateHandlers.Add(
        NAME("OnClientDisconnected"),
        g_gameServer->GetNetServer().OnClientDisconnected.BindThreaded(
            [this](const NetServerConnectionStateChangedData& data)
            {
                HandleClientDisconnected(data.connectionId);
            }, g_simThread));
}

void PlayerSystem::OnRemovedFromWorld(World* world)
{
    m_delegateHandlers.Remove("OnConnected"_sh);
    m_delegateHandlers.Remove("OnDisconnected"_sh);

    if (EngineGlobals::IsServer())
    {
        m_delegateHandlers.Remove("OnClientConnected"_sh);
        m_delegateHandlers.Remove("OnClientDisconnected"_sh);
    }

    SystemBase::OnRemovedFromWorld(world);
}

bool PlayerSystem::TrySpawnPlayerEntity(net::NetConnectionId connectionId)
{
    // For now just take the first template
    // Later on we'll need some way of telling or something

    Assert(!m_playerEntityTemplates.Empty(), "No player entity templates");

    auto it = *m_playerEntityTemplates.Begin();

    const Handle<Entity>& templateEntity = it.second;
    Assert(templateEntity.IsValid());

    Handle<Entity> clone = DynamicCast<Entity>(templateEntity->Clone());

    if (!clone.IsValid())
    {
        HYP_LOG(Replication, Error, "Failed to clone player template entity for connection id {}", uint32(connectionId));

        return true;
    }

    Node* parent = templateEntity->GetParent();

    if (parent)
    {
        parent->AddChild(clone);
    }
    else
    {
        templateEntity->GetEntityManager()->GetScene()->GetRoot()->AddChild(clone);
    }

    clone->AddComponent<PlayerComponent>(PlayerComponent { connectionId });
    clone->AddTag<EntityTag::Replicated>();

    m_connectionIdToClone.Set(connectionId, clone);

    const Vec3f worldTranslation = clone->GetWorldTranslation();

    Handle<CameraStreamingVolume> streamingVolume = MakeHandle<CameraStreamingVolume>();
    streamingVolume->SetBoundingBox(BoundingBox(worldTranslation - 10.0f, worldTranslation + 10.0f));
    InitObject(streamingVolume);

    g_streamingManager->AddStreamingVolume(streamingVolume);

    m_connectionIdToStreamingVolume.Set(connectionId, streamingVolume);

    HYP_LOG(Replication, Info, "Cloned player entity for connection id {}", uint32(connectionId));

    return true;
}

void PlayerSystem::HandleClientConnected(net::NetConnectionId connectionId)
{
    if (!TrySpawnPlayerEntity(connectionId))
    {
        HYP_LOG(Replication, Info,
            "Client connected (connection id: {}) before the player template entity was resolved -- queued, will retry",
            uint32(connectionId));

        m_pendingConnections.PushBack(connectionId);
    }
}

void PlayerSystem::HandleClientDisconnected(net::NetConnectionId connectionId)
{
    if (auto pendingIt = m_pendingConnections.Find(connectionId); pendingIt != m_pendingConnections.End())
    {
        HYP_LOG(Replication, Error,
            "Client (connection id: {}) disconnected before a player clone could be created for it -- giving up",
            uint32(connectionId));

        m_pendingConnections.Erase(pendingIt);
    }

    auto it = m_connectionIdToClone.Find(connectionId);

    if (it != m_connectionIdToClone.End())
    {
        // remove the component
        it->second->RemoveComponent<PlayerComponent>();

        // remove from parent
        it->second->Remove();

        m_connectionIdToClone.Erase(it);
    }

    if (auto volumeIt = m_connectionIdToStreamingVolume.Find(connectionId); volumeIt != m_connectionIdToStreamingVolume.End())
    {
        g_streamingManager->RemoveStreamingVolume(volumeIt->second);

        m_connectionIdToStreamingVolume.Erase(volumeIt);
    }
}

void PlayerSystem::UpdateStreamingVolumes()
{
    for (auto& [connectionId, volume] : m_connectionIdToStreamingVolume)
    {
        auto cloneIt = m_connectionIdToClone.Find(connectionId);

        if (cloneIt == m_connectionIdToClone.End())
        {
            continue;
        }

        const Vec3f worldTranslation = cloneIt->second->GetWorldTranslation();

        volume->SetBoundingBox(BoundingBox(worldTranslation - 10.0f, worldTranslation + 10.0f));
    }
}

void PlayerSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (m_playerEntityTemplates.Any() && m_pendingConnections.Any())
    {
        for (size_t i = 0; i < m_pendingConnections.Size();)
        {
            if (TrySpawnPlayerEntity(m_pendingConnections[i]))
            {
                m_pendingConnections.EraseAt(i);

                continue;
            }

            ++i;
        }
    }

    UpdateStreamingVolumes();
}

} // namespace Hyperion
