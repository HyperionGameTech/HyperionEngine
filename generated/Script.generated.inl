#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region ScriptData Reflection Data

HYP_BEGIN_STRUCT(ScriptData, 230, 0, {})
    Field(NAME(HYP_STR(Uuid)), &ScriptData::uuid, offsetof(ScriptData, uuid), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Language)), &ScriptData::language, offsetof(ScriptData, language), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Path)), &ScriptData::path, offsetof(ScriptData, path), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(AssemblyPath)), &ScriptData::assemblyPath, offsetof(ScriptData, assemblyPath), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(ClassName)), &ScriptData::className, offsetof(ScriptData, className), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(CompileStatus)), &ScriptData::compileStatus, offsetof(ScriptData, compileStatus), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(HotReloadVersion)), &ScriptData::hotReloadVersion, offsetof(ScriptData, hotReloadVersion), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(LastModifiedTimestamp)), &ScriptData::lastModifiedTimestamp, offsetof(ScriptData, lastModifiedTimestamp), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion ScriptData Reflection Data

} // namespace hyperion

