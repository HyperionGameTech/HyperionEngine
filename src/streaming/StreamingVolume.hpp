/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <core/containers/Array.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <core/object/HypObject.hpp>

#include <core/threading/Semaphore.hpp>

#include <GameCounter.hpp>

namespace hyperion {

class StreamingNotifier;

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
class HYP_API StreamingVolumeBase : public HypObjectBase
{
    HYP_OBJECT_BODY(StreamingVolumeBase);

public:
    StreamingVolumeBase() = default;
    StreamingVolumeBase(const StreamingVolumeBase& other) = delete;
    StreamingVolumeBase& operator=(const StreamingVolumeBase& other) = delete;
    StreamingVolumeBase(StreamingVolumeBase&& other) noexcept = delete;
    StreamingVolumeBase& operator=(StreamingVolumeBase&& other) noexcept = delete;
    virtual ~StreamingVolumeBase() override = default;

    void RegisterNotifier(StreamingNotifier* notifier)
    {
        Assert(notifier != nullptr);
        Assert(!m_notifiers.Contains(notifier));

        m_notifiers.PushBack(notifier);
    }

    void UnregisterNotifier(StreamingNotifier* notifier)
    {
        if (!notifier) {
            return;
        }

        m_notifiers.Erase(notifier);
    }

    HYP_METHOD(Scriptable)
    StreamingVolumeShape GetShape() const;

    HYP_METHOD(Scriptable)
    bool GetBoundingBox(BoundingBox& outAabb) const;

    HYP_METHOD(Scriptable)
    bool GetBoundingSphere(BoundingSphere& outSphere) const;

    HYP_METHOD(Scriptable)
    bool ContainsPoint(const Vec3f& point) const;

protected:
    HYP_METHOD()
    virtual StreamingVolumeShape GetShape_Impl() const
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    virtual bool GetBoundingBox_Impl(BoundingBox& outAabb) const
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    virtual bool GetBoundingSphere_Impl(BoundingSphere& outSphere) const
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    virtual bool ContainsPoint_Impl(const Vec3f& point) const
    {
        HYP_PURE_VIRTUAL();
    }

    /*! \brief Notify all registered notifiers that the volume has been updated.
     *  This is typically called when the volume's bounding box or shape changes and needs have the changes be reflected in the streaming system. */
    HYP_METHOD()
    void NotifyUpdate();

private:
    Array<StreamingNotifier*> m_notifiers;
};

} // namespace hyperion

