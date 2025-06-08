/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMING_MANAGER_HPP
#define HYPERION_STREAMING_MANAGER_HPP

#include <streaming/Streamable.hpp>

#include <core/Base.hpp>
#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/Scheduler.hpp>
#include <core/threading/Mutex.hpp>

#include <core/utilities/UUID.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <GameCounter.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

namespace threading {

class IThread;
class TaskThreadPool;

} // namespace threading

using threading::IThread;
using threading::TaskThreadPool;

class StreamingManagerThread;
class StreamingThreadPool;
class StreamingPool;
class StreamingCell;

enum class IterationResult : uint8;

HYP_ENUM()
enum class StreamingVolumeShape : uint32
{
    SPHERE = 0,
    BOX,

    MAX,

    INVALID = ~0u
};

HYP_CLASS(Abstract)
class HYP_API StreamingVolumeBase : public HypObject<StreamingVolumeBase>
{
    HYP_OBJECT_BODY(StreamingVolumeBase);

public:
    StreamingVolumeBase() = default;
    StreamingVolumeBase(const StreamingVolumeBase& other) = delete;
    StreamingVolumeBase& operator=(const StreamingVolumeBase& other) = delete;
    StreamingVolumeBase(StreamingVolumeBase&& other) noexcept = delete;
    StreamingVolumeBase& operator=(StreamingVolumeBase&& other) noexcept = delete;
    virtual ~StreamingVolumeBase() = default;

    HYP_METHOD(Scriptable)
    StreamingVolumeShape GetShape() const;

    HYP_METHOD(Scriptable)
    bool GetBoundingBox(BoundingBox& out_aabb) const;

    HYP_METHOD(Scriptable)
    bool GetBoundingSphere(BoundingSphere& out_sphere) const;

    HYP_METHOD(Scriptable)
    bool ContainsPoint(const Vec3f &point) const;

protected:
    HYP_METHOD()
    virtual StreamingVolumeShape GetShape_Impl() const
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    virtual bool GetBoundingBox_Impl(BoundingBox& out_aabb) const
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    virtual bool GetBoundingSphere_Impl(BoundingSphere& out_sphere) const
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    virtual bool ContainsPoint_Impl(const Vec3f &point) const
    {
        HYP_PURE_VIRTUAL();
    }
};

HYP_CLASS()
class HYP_API StreamingManager : public HypObject<StreamingManager>
{
    HYP_OBJECT_BODY(StreamingManager);

public:
    friend void PostStreamingManagerUpdate(StreamingManager*, Proc<void()>&&);
    friend void ForEachStreamingVolume(StreamingManager*, ProcRef<IterationResult(StreamingVolumeBase*)>);

    StreamingManager();
    StreamingManager(const StreamingManager& other) = delete;
    StreamingManager& operator=(const StreamingManager& other) = delete;
    StreamingManager(StreamingManager&& other) noexcept = delete;
    StreamingManager& operator=(StreamingManager&& other) noexcept = delete;
    ~StreamingManager();

    HYP_METHOD()
    void AddStreamingVolume(const Handle<StreamingVolumeBase>& volume);

    HYP_METHOD()
    void RemoveStreamingVolume(const Handle<StreamingVolumeBase>& volume);

    HYP_METHOD()
    void RegisterCell(const Handle<StreamingCell>& cell);

    HYP_METHOD()
    void UnregisterCell(const Handle<StreamingCell>& cell);

    HYP_METHOD()
    void RegisterStreamable(const Handle<StreamableBase>& streamable);

    HYP_METHOD()
    void UnregisterStreamable(const UUID& uuid);

    HYP_METHOD()
    void UnregisterAllStreamables();

    void Init();
    void Start();
    void Stop();
    void Update(GameCounter::TickUnit delta);

private:
    UniquePtr<StreamingManagerThread> m_thread;

    // For streaming thread to post updates to
    Scheduler m_scheduler;

    Array<Handle<StreamingVolumeBase>> m_streaming_volumes;
    mutable Mutex m_streaming_volumes_mutex;
};

void PostStreamingManagerUpdate(StreamingManager*, Proc<void()>&&);
void ForEachStreamingVolume(StreamingManager*, ProcRef<IterationResult(StreamingVolumeBase*)>);

} // namespace hyperion

#endif
