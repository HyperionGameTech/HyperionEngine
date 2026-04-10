#pragma once

#include "Backend/RxCommands.hpp"

#include <Core/FxPagedArray.hpp>

enum class RxCommandUsage
{
    eGraphics,
    eTransfer
};

using RxCommandHandle = FxHandle;

class RxCommandManager
{
public:
    RxCommandManager() {}

    RxCommandHandle Request(RxCommandUsage usage);

    void Release(RxCommandHandle handle);

private:
};
