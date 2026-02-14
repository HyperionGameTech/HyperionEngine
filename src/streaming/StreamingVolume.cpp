/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <StreamingPch.hpp>

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
