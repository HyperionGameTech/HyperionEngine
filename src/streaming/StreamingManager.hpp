/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/containers/Array.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/ThreadSignal.hpp>

#include <core/utilities/Uuid.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <streaming/Streamable.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

class StreamingManagerThread;
class StreamingVolumeBase;
class WorldGrid;
class WorldGridLayer;

class StreamingNotifier final : public ThreadSignal
{
};

HYP_CLASS()
class HYP_API StreamingManager final : public ObjectBase
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

} // namespace Hyperion
