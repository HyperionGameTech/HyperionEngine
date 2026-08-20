/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Components/PlayerComponent.hpp>

#include <Framework/Client/ClientReplicationManager.hpp>// for ReplicationOp

#include <Core/Containers/Map.hpp>

#include <Core/Math/Transform.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

enum class NetId : uint32;
class CameraStreamingVolume;

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

    Handle<Entity> GetMyPlayerEntity() const;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<PlayerComponent, ComponentAccess::READ_WRITE, false> {}
        };
    }

private:
    static constexpr float PendingSpawnTimeoutSeconds = 5.0f;

    struct PendingSpawn
    {
        ReplicationOp<ReplicationOpType::Spawn> spawnOp;
        float secondsWaited = 0.0f;
    };

    void TryResolvePendingSpawns(Span<Handle<Scene>> scenes);

    void UpdateStreamingVolume(Span<Handle<Scene>> scenes);

    Array<PendingSpawn, SceneAllocator> m_pendingSpawns;
    Map<NetId, Handle<Entity>, SceneAllocator> m_netIdToEntity;

    Handle<Entity> m_resolvedPlayerEntity;
    Handle<CameraStreamingVolume> m_playerStreamingVolume;
};

} // namespace Hyperion
