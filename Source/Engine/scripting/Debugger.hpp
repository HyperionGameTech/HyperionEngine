/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

namespace Hyperion {

/// For interacting with the debugger from scripts - Not intended for use in native code.
HYP_CLASS(OnlyLanguages = "hypscript")
class HYP_API Debugger final : public ObjectBase
{
    HYP_OBJECT_BODY(Debugger);

public:
    HYP_METHOD()
    static void Breakpoint();

    HYP_METHOD()
    static void AssertTrue(bool condition);

    HYP_METHOD()
    static void AssertFalse(bool condition);
};

} // namespace Hyperion
