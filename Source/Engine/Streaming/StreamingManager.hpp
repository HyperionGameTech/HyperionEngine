/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Threading/ThreadSignal.hpp>

#include <Core/Logging/LoggerFwd.hpp>

#include <Streaming/Streamable.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Streaming);

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
class ENGINE_API StreamingManager final : public ObjectBase
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
    UniquePtr<StreamingManagerThread> m_thread;
};

} // namespace Hyperion
