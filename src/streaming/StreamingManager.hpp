/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMING_MANAGER_HPP
#define HYPERION_STREAMING_MANAGER_HPP

#include <streaming/Streamable.hpp>

#include <core/Base.hpp>
#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/containers/Array.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/Semaphore.hpp>

#include <core/utilities/UUID.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <GameCounter.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

class StreamingManagerThread;
class StreamingVolumeBase;
class WorldGrid;
class WorldGridLayer;

class StreamingNotifier final : public Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>
{
};

HYP_CLASS()
class HYP_API StreamingManager final : public HypObject<StreamingManager>
{
    HYP_OBJECT_BODY(StreamingManager);

public:
    StreamingManager();
    StreamingManager(const WeakHandle<WorldGrid>& world_grid);
    StreamingManager(const StreamingManager& other) = delete;
    StreamingManager& operator=(const StreamingManager& other) = delete;
    StreamingManager(StreamingManager&& other) noexcept = delete;
    StreamingManager& operator=(StreamingManager&& other) noexcept = delete;
    ~StreamingManager() override;

    HYP_FORCE_INLINE const WeakHandle<WorldGrid>& GetWorldGrid() const
    {
        return m_world_grid;
    }

    HYP_METHOD()
    void AddStreamingVolume(const Handle<StreamingVolumeBase>& volume);

    HYP_METHOD()
    void RemoveStreamingVolume(StreamingVolumeBase* volume);

    void AddWorldGridLayer(const Handle<WorldGridLayer>& layer);
    void RemoveWorldGridLayer(WorldGridLayer* layer);

    void Start();
    void Stop();
    void Update(float delta);

private:
    void Init() override;

    WeakHandle<WorldGrid> m_world_grid;

    UniquePtr<StreamingManagerThread> m_thread;
};

} // namespace hyperion

#endif
