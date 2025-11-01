/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <streaming/Streamable.hpp>

#include <core/reflection/Handle.hpp>
#include <core/Defines.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/containers/Array.hpp>

#include <core/reflection/HypObject.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/Semaphore.hpp>

#include <core/utilities/Uuid.hpp>

#include <core/logging/LoggerFwd.hpp>

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
class HYP_API StreamingManager final : public HypObjectBase
{
    HYP_OBJECT_BODY(StreamingManager);

public:
    StreamingManager();

    StreamingManager(const StreamingManager& other) = delete;
    StreamingManager& operator=(const StreamingManager& other) = delete;

    StreamingManager(StreamingManager&& other) noexcept = delete;
    StreamingManager& operator=(StreamingManager&& other) noexcept = delete;

    ~StreamingManager() override;

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

    UniquePtr<StreamingManagerThread> m_thread;
};

} // namespace hyperion
