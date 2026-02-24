/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <scripting/Debugger.hpp>

#include <Debugger.generated.inl>

namespace Hyperion {

void Debugger::Breakpoint()
{
    HYP_BREAKPOINT;
}

void Debugger::AssertTrue(bool condition)
{
    AssertDebug(condition, "Assertion failed: condition is false");
}

void Debugger::AssertFalse(bool condition)
{
    AssertDebug(!condition, "Assertion failed: condition is true");
}

} // namespace Hyperion
