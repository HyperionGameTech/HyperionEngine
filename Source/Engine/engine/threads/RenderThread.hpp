/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

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