/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>

#include <Framework/Client/ClientReplicationManager.hpp>// for ReplicationOp

#include <Core/Containers/Map.hpp>

#include <Core/Math/Transform.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

enum class NetId : uint32;

HYP_CLASS(NoScriptBindings)
class ReplicationApplySystem final : public SystemBase
{
    HYP_OBJECT_BODY(ReplicationApplySystem);

public:
    ~ReplicationApplySystem() override = default;

    bool RequiresSimThread() const override
    {
        return true;
    }

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {};
    }

private:
    static constexpr float PendingSpawnTimeoutSeconds = 5.0f;

    struct PendingSpawn
    {
        ReplicationOp<ReplicationOpType::Spawn> spawnOp;
        float secondsWaited = 0.0f;
    };

    void TryResolvePendingSpawns(Scene* scene);

    Array<PendingSpawn, SceneAllocator> m_pendingSpawns;
    Map<NetId, Handle<Entity>, SceneAllocator> m_netIdToEntity;
};

} // namespace Hyperion
