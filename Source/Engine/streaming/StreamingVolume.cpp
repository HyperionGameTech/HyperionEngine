/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <streaming/StreamingVolume.hpp>
#include <streaming/StreamingManager.hpp>

#include <StreamingVolume.generated.inl>

namespace Hyperion {

void StreamingVolumeBase::NotifyUpdate()
{
    Mutex::Guard guard(m_notifiersMtx);

    for (StreamingNotifier* notifier : m_notifiers)
    {
        Assert(notifier != nullptr);

        notifier->Signal();
    }
}

} // namespace Hyperion
