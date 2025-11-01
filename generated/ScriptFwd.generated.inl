#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region ScriptCompileStatus Reflection Data

HYP_BEGIN_ENUM(ScriptCompileStatus, 399, 0, {})
    HypConstant(NAME(HYP_STR(SCS_UNINITIALIZED)), ScriptCompileStatus::SCS_UNINITIALIZED),
    HypConstant(NAME(HYP_STR(SCS_COMPILED)), ScriptCompileStatus::SCS_COMPILED),
    HypConstant(NAME(HYP_STR(SCS_DIRTY)), ScriptCompileStatus::SCS_DIRTY),
    HypConstant(NAME(HYP_STR(SCS_PROCESSING)), ScriptCompileStatus::SCS_PROCESSING),
    HypConstant(NAME(HYP_STR(SCS_ERRORED)), ScriptCompileStatus::SCS_ERRORED)
HYP_END_ENUM

#pragma endregion ScriptCompileStatus Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ScriptLanguage Reflection Data

HYP_BEGIN_ENUM(ScriptLanguage, 400, 0, {})
    HypConstant(NAME(HYP_STR(SL_INVALID)), ScriptLanguage::SL_INVALID),
    HypConstant(NAME(HYP_STR(SL_NATIVE)), ScriptLanguage::SL_NATIVE),
    HypConstant(NAME(HYP_STR(SL_HYPSCRIPT)), ScriptLanguage::SL_HYPSCRIPT),
    HypConstant(NAME(HYP_STR(SL_CSHARP)), ScriptLanguage::SL_CSHARP)
HYP_END_ENUM

#pragma endregion ScriptLanguage Reflection Data

} // namespace hyperion

