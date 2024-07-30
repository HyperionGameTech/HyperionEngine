/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/net/NetRequestThread.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion::net {

NetRequestThread::NetRequestThread()
    : TaskThread(NAME("NetRequestThread"), ThreadPriorityValue::LOWEST)
{
}

NetRequestThread::~NetRequestThread() = default;

} // namespace hyperion::net