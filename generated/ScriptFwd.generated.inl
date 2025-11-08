#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region ScriptCompileStatus Reflection Data

HYP_BEGIN_ENUM(ScriptCompileStatus, 393, 0, {})
    StaticField(NAME(HYP_STR(SCS_UNINITIALIZED)), ScriptCompileStatus::SCS_UNINITIALIZED),
    StaticField(NAME(HYP_STR(SCS_COMPILED)), ScriptCompileStatus::SCS_COMPILED),
    StaticField(NAME(HYP_STR(SCS_DIRTY)), ScriptCompileStatus::SCS_DIRTY),
    StaticField(NAME(HYP_STR(SCS_PROCESSING)), ScriptCompileStatus::SCS_PROCESSING),
    StaticField(NAME(HYP_STR(SCS_ERRORED)), ScriptCompileStatus::SCS_ERRORED)
HYP_END_ENUM

#pragma endregion ScriptCompileStatus Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ScriptLanguage Reflection Data

HYP_BEGIN_ENUM(ScriptLanguage, 394, 0, {})
    StaticField(NAME(HYP_STR(SL_INVALID)), ScriptLanguage::SL_INVALID),
    StaticField(NAME(HYP_STR(SL_NATIVE)), ScriptLanguage::SL_NATIVE),
    StaticField(NAME(HYP_STR(SL_HYPSCRIPT)), ScriptLanguage::SL_HYPSCRIPT),
    StaticField(NAME(HYP_STR(SL_CSHARP)), ScriptLanguage::SL_CSHARP)
HYP_END_ENUM

#pragma endregion ScriptLanguage Reflection Data

} // namespace hyperion

