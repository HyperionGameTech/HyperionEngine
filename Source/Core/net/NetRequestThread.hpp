/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/TaskThread.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace Hyperion {
namespace net {

class HYP_API NetRequestThread final : public TaskThread
{
public:
    NetRequestThread();
    virtual ~NetRequestThread() override;
};

HYP_API void SetGlobalNetRequestThread(const RC<NetRequestThread>& netRequestThread);
HYP_API const RC<NetRequestThread>& GetGlobalNetRequestThread();

} // namespace net

using net::GetGlobalNetRequestThread;
using net::NetRequestThread;
using net::SetGlobalNetRequestThread;

} // namespace Hyperion
