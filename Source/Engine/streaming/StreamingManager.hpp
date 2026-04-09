/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/containers/Array.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/threading/ThreadSignal.hpp>

#include <Core/logging/LoggerFwd.hpp>

#include <streaming/Streamable.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

class StreamingManagerThread;
class StreamingVolumeBase;
class WorldGrid;
class WorldGridLayer;

class StreamingNotifier final : public ThreadSignal
{
public:
    StreamingNotifier()
        : ThreadSignal(false)
    {
    }
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
