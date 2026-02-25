/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/net/NetRequestThread.hpp>

#include <Core/threading/Mutex.hpp>

namespace Hyperion::net {

static RC<NetRequestThread> g_globalNetRequestThread;
static Mutex g_globalNetRequestThreadMutex;

HYP_API void SetGlobalNetRequestThread(const RC<NetRequestThread>& netRequestThread)
{
    Mutex::Guard guard(g_globalNetRequestThreadMutex);

    g_globalNetRequestThread = netRequestThread;
}

HYP_API const RC<NetRequestThread>& GetGlobalNetRequestThread()
{
    Mutex::Guard guard(g_globalNetRequestThreadMutex);

    return g_globalNetRequestThread;
}

NetRequestThread::NetRequestThread()
    : TaskThread(NAME("NetRequestThread"), ThreadPriorityValue::LOWEST)
{
}

NetRequestThread::~NetRequestThread() = default;

} // namespace Hyperion::net