/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamingVolume.hpp>
#include <streaming/StreamingManager.hpp>

namespace hyperion {

void StreamingVolumeBase::NotifyUpdate()
{
    for (StreamingNotifier* notifier : m_notifiers)
    {
        AssertThrow(notifier != nullptr);

        notifier->Produce();
    }
}

} // namespace hyperion