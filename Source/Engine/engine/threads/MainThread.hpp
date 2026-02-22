/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/threading/Thread.hpp>
#include <Core/threading/Scheduler.hpp>

namespace Hyperion {

class MainThread final : public Thread<Scheduler>
{
public:
    MainThread();
    ~MainThread();

    bool Start();
    virtual void Stop() override;

    void Update();

private:
    virtual void operator()() override;
};

} // namespace Hyperion