/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/ObjectMacros.hpp>
#include <Core/reflection/ObjId.hpp>
#include <Core/reflection/Handle.hpp>

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
