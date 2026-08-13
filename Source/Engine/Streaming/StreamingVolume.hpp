/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/BoundingSphere.hpp>

#include <Core/Threading/Semaphore.hpp>
#include <Core/Threading/Mutex.hpp>

namespace Hyperion {

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
class ENGINE_API StreamingVolumeBase : public ObjectBase
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

        Mutex::Guard guard(m_notifiersMtx);

        Assert(!m_notifiers.Contains(notifier));

        m_notifiers.PushBack(notifier);
    }

    void UnregisterNotifier(StreamingNotifier* notifier)
    {
        if (!notifier)
        {
            return;
        }

        Mutex::Guard guard(m_notifiersMtx);

        m_notifiers.Erase(notifier);
    }

    HYP_METHOD()
    virtual StreamingVolumeShape GetShape() const = 0;

    HYP_METHOD()
    virtual bool GetBoundingBox(BoundingBox& outAabb) const = 0;

    HYP_METHOD()
    virtual bool GetBoundingSphere(BoundingSphere& outSphere) const = 0;

    HYP_METHOD()
    virtual bool ContainsPoint(const Vec3f& point) const = 0;

    /*! \brief Notify all registered notifiers that the volume has been updated.
     *  This is typically called when the volume's bounding box or shape changes and needs have the changes be reflected in the streaming system. */
    HYP_METHOD()
    void NotifyUpdate();

private:
    Mutex m_notifiersMtx;
    Array<StreamingNotifier*> m_notifiers;
};

} // namespace Hyperion
