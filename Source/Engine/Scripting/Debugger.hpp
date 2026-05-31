/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/ObjectMacros.hpp>
#include <Core/Reflection/ObjId.hpp>
#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

/// For interacting with the debugger from scripts - Not intended for use in native code.
HYP_CLASS(OnlyLanguages = "hypscript")
class ENGINE_API Debugger final : public ObjectBase
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
