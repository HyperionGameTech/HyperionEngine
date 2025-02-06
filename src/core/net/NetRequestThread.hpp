/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_NET_NET_REQUEST_THREAD_HPP
#define HYPERION_CORE_NET_NET_REQUEST_THREAD_HPP

#include <core/threading/TaskThread.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {
namespace net {

class HYP_API NetRequestThread final : public TaskThread
{
public:
    NetRequestThread();
    virtual ~NetRequestThread() override;
};

HYP_API void SetGlobalNetRequestThread(const RC<NetRequestThread> &net_request_thread);
HYP_API const RC<NetRequestThread> &GetGlobalNetRequestThread();

} // namespace net

using net::NetRequestThread;
using net::SetGlobalNetRequestThread;
using net::GetGlobalNetRequestThread;

} // namespace hyperion

#endif // HYPERION_CORE_NET_NET_REQUEST_THREAD_HPP