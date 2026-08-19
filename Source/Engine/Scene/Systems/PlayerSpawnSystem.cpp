/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/PlayerSpawnSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Camera/Camera.hpp>

#include <Scene/Components/ReplicationStateComponent.hpp>

#include <Framework/Server/GameServer.hpp>

#include <Net/NetServer.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>

#include <Core/Logging/Logger.hpp>

#include <PlayerSpawnSystem.generated.inl>

namespace Hyperion {

void PlayerSpawnSystem::OnAddedToWorld(World* world)
{
    SystemBase::OnAddedToWorld(world);

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

void PlayerSpawnSystem::OnRemovedFromWorld(World* world)
{
    m_delegateHandlers.Remove("OnClientConnected"_sh);
    m_delegateHandlers.Remove("OnClientDisconnected"_sh);

    SystemBase::OnRemovedFromWorld(world);
}

void PlayerSpawnSystem::ResolveTemplate(Span<Handle<Scene>> scenes)
{
    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, tag] : scene->GetEntityManager()->GetEntitySet<TagComponent<EntityTag::Player>>())
        {
            m_templateEntity = MakeStrongRef(entity);

            HYP_LOG(Replication, Info, "PlayerSpawnSystem resolved template player entity '{}' in scene '{}'",
                entity->GetName(), scene->GetName());

            return;
        }
    }
}

bool PlayerSpawnSystem::TrySpawnClone(net::NetConnectionId connectionId)
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

        // Something wrong with it, don't retry.
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

    HYP_LOG(Replication, Info, "Cloned player entity for connection id {}", uint32(connectionId));

    return true;
}

void PlayerSpawnSystem::HandleClientConnected(net::NetConnectionId connectionId)
{
    if (!m_templateEntity.IsValid())
    {
        if (World* world = GetWorld())
        {
            Array<Handle<Scene>, SceneAllocator> scenes(world->GetScenes());
            ResolveTemplate(scenes);
        }
    }

    if (!TrySpawnClone(connectionId))
    {
        HYP_LOG(Replication, Warning,
            "Client connected (connection id: {}) before the player template entity was resolved -- retrying once it's available",
            uint32(connectionId));

        m_pendingConnections.PushBack(connectionId);
    }
}

void PlayerSpawnSystem::HandleClientDisconnected(net::NetConnectionId connectionId)
{
    auto it = m_connectionIdToClone.Find(connectionId);

    if (it == m_connectionIdToClone.End())
    {
        return;
    }

    it->second->Remove();

    m_connectionIdToClone.Erase(it);
}

void PlayerSpawnSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (!m_templateEntity.IsValid())
    {
        ResolveTemplate(scenes);
    }

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
}

} // namespace Hyperion
