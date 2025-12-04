/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

namespace hyperion {

class RenderThread final : public Thread<Scheduler>
{
public:
    RenderThread();
    ~RenderThread();

    bool Start();
    void Update();

private:
    virtual void operator()() override;
};

} // namespace hyperion