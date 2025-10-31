#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region ScriptData Reflection Data

HYP_BEGIN_STRUCT(ScriptData, 229, 0, {})
    HypField(NAME(HYP_STR(Uuid)), &ScriptData::uuid, offsetof(ScriptData, uuid), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Language)), &ScriptData::language, offsetof(ScriptData, language), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Path)), &ScriptData::path, offsetof(ScriptData, path), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(AssemblyPath)), &ScriptData::assemblyPath, offsetof(ScriptData, assemblyPath), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(ClassName)), &ScriptData::className, offsetof(ScriptData, className), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(CompileStatus)), &ScriptData::compileStatus, offsetof(ScriptData, compileStatus), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(HotReloadVersion)), &ScriptData::hotReloadVersion, offsetof(ScriptData, hotReloadVersion), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(LastModifiedTimestamp)), &ScriptData::lastModifiedTimestamp, offsetof(ScriptData, lastModifiedTimestamp), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion ScriptData Reflection Data

} // namespace hyperion

