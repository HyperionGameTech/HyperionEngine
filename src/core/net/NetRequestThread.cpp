/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/net/NetRequestThread.hpp>

#include <core/threading/Mutex.hpp>

namespace hyperion::net {

static RC<NetRequestThread> s_globalNetRequestThread;
static Mutex s_globalNetRequestThreadMutex;

HYP_API void SetGlobalNetRequestThread(const RC<NetRequestThread>& netRequestThread)
{
    Mutex::Guard guard(s_globalNetRequestThreadMutex);

    s_globalNetRequestThread = netRequestThread;
}

HYP_API const RC<NetRequestThread>& GetGlobalNetRequestThread()
{
    Mutex::Guard guard(s_globalNetRequestThreadMutex);

    return s_globalNetRequestThread;
}

NetRequestThread::NetRequestThread()
    : TaskThread(NAME("NetRequestThread"), ThreadPriorityValue::LOWEST)
{
}

NetRequestThread::~NetRequestThread() = default;

} // namespace hyperion::net