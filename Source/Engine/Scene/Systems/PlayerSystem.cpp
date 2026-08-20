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

    if (!entity->HasComponent<PlayerComponent>())
    {
        entity->AddComponent<PlayerComponent>(PlayerComponent {});
    }

    // The template is whichever Player-tagged entity is discovered first, Player tag is stripped in TrySpawnClone.
    if (EngineGlobals::IsServer() && !m_templateEntity.IsValid())
    {
        m_templateEntity = MakeStrongRef(entity);

        HYP_LOG(Replication, Info, "PlayerSystem resolved template player entity '{}' in scene '{}'",
            entity->GetName(), entity->GetEntityManager()->GetScene()->GetName());
    }
}

void PlayerSystem::OnAddedToWorld(World* world)
{
    SystemBase::OnAddedToWorld(world);

    if (!EngineGlobals::IsServer())
    {
        return;
    }

    Assert(g_gameServer != nullptr);

    m_delegateHandlers.Add(
        NAME("OnClientConnected"),
        g_gameServer->GetNetServer().OnClientConnected.Bind(
            [this](const NetClientConnectedData& data)
            {
                GetThreadById(g_simThread)->GetScheduler().Enqueue(
                    [this, connectionId = data.connectionId]()
                    {
                        HandleClientConnected(connectionId);
                    },
                    TaskEnqueueFlags::FIRE_AND_FORGET);
            }));

    m_delegateHandlers.Add(
        NAME("OnClientDisconnected"),
        g_gameServer->GetNetServer().OnClientDisconnected.Bind(
            [this](const NetClientDisconnectedData& data)
            {
                GetThreadById(g_simThread)->GetScheduler().Enqueue(
                    [this, connectionId = data.connectionId]()
                    {
                        HandleClientDisconnected(connectionId);
                    },
                    TaskEnqueueFlags::FIRE_AND_FORGET);
            }));
}

void PlayerSystem::OnRemovedFromWorld(World* world)
{
    if (EngineGlobals::IsServer())
    {
        m_delegateHandlers.Remove("OnClientConnected"_sh);
        m_delegateHandlers.Remove("OnClientDisconnected"_sh);
    }

    SystemBase::OnRemovedFromWorld(world);
}

bool PlayerSystem::TrySpawnClone(net::NetConnectionId connectionId)
{
    if (!m_templateEntity.IsValid())
    {
        return false;
    }

    Handle<Node> clonedNode = m_templateEntity->Clone();
    Handle<Entity> clone = DynamicCast<Entity>(clonedNode);

    if (!clone.IsValid())
    {
        HYP_LOG(Replication, Error, "Failed to clone player template entity for connection id {}", uint32(connectionId));

        return true;
    }

    // strip the cloned Camera child from the Player if one exists.
    //  - clients attach their own camera locally once they resolve their own clone.
    for (const Handle<Node>& child : Array<Handle<Node>>(clone->GetChildren()))
    {
        if (DynamicCast<Camera>(child))
        {
            child->Remove();
        }
    }

    Node* parent = m_templateEntity->GetParent();

    if (parent)
    {
        parent->AddChild(clone);
    }
    else
    {
        m_templateEntity->GetEntityManager()->GetScene()->GetRoot()->AddChild(clone);
    }

    clone->GetComponent<PlayerComponent>().connectionId = connectionId;

    m_connectionIdToClone.Set(connectionId, clone);

    clone->RemoveTag<EntityTag::Player>();
    clone->AddTag<EntityTag::Replicated>();

    // The server never has a Camera to hang a streaming volume off of (stripped above) --
    // the clone's own translation drives its connection's streaming interest directly.
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
    if (!TrySpawnClone(connectionId))
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
    if (m_templateEntity.IsValid() && m_pendingConnections.Any())
    {
        for (size_t i = 0; i < m_pendingConnections.Size();)
        {
            if (TrySpawnClone(m_pendingConnections[i]))
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
