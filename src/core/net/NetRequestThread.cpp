/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/net/NetRequestThread.hpp>

#include <core/threading/Mutex.hpp>

namespace hyperion::net {

static RC<NetRequestThread> g_global_net_request_thread;;
static Mutex g_global_net_request_thread_mutex;

HYP_API void SetGlobalNetRequestThread(const RC<NetRequestThread> &net_request_thread)
{
    Mutex::Guard guard(g_global_net_request_thread_mutex);

    g_global_net_request_thread = net_request_thread;
}

HYP_API const RC<NetRequestThread> &GetGlobalNetRequestThread()
{
    Mutex::Guard guard(g_global_net_request_thread_mutex);

    return g_global_net_request_thread;
}

NetRequestThread::NetRequestThread()
    : TaskThread(NAME("NetRequestThread"), ThreadPriorityValue::LOWEST)
{
}

NetRequestThread::~NetRequestThread() = default;

} // namespace hyperion::net