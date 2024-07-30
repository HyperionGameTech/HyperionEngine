/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_NET_NET_REQUEST_THREAD_HPP
#define HYPERION_CORE_NET_NET_REQUEST_THREAD_HPP

#include <core/threading/TaskThread.hpp>

namespace hyperion::net {

class HYP_API NetRequestThread final : public TaskThread
{
public:
    NetRequestThread();
    virtual ~NetRequestThread() override;
};

} // namespace hyperion::net

#endif // HYPERION_CORE_NET_NET_REQUEST_THREAD_HPP