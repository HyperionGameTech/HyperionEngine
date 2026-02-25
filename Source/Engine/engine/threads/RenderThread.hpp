/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/threading/Thread.hpp>
#include <Core/threading/Scheduler.hpp>

namespace Hyperion {

class RenderThread final : public Thread<Scheduler>
{
public:
    RenderThread();
    ~RenderThread() override;

    bool Start();
    void Stop() override;

    void Update();

private:
    virtual void operator()() override;
};

} // namespace Hyperion