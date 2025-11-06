#pragma once

#include <scripting/ScriptFwd.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Uuid.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/reflection/ObjectMacros.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

struct ScriptDesc
{
    FilePath path;
};

static constexpr SizeType scriptMaxPathLength = 1024;
static constexpr SizeType scriptMaxClassNameLength = 1024;

HYP_STRUCT()
struct ScriptData
{
    HYP_STRUCT_BODY(ScriptData);

    HYP_FIELD(Serialize)
    Uuid uuid;

    HYP_FIELD(Serialize)
    ScriptLanguage language = SL_HYPSCRIPT;

    HYP_FIELD(Serialize)
    FixedArray<char, scriptMaxPathLength> path;

    HYP_FIELD(Serialize)
    FixedArray<char, scriptMaxPathLength> assemblyPath; // C# only

    HYP_FIELD(Serialize)
    FixedArray<char, scriptMaxClassNameLength> className;

    HYP_FIELD(Serialize)
    uint32 compileStatus;

    HYP_FIELD(Serialize)
    int32 hotReloadVersion;

    HYP_FIELD(Serialize)
    uint64 lastModifiedTimestamp;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(uuid);
        hashCode.Add(language);
        hashCode.Add(HashCode::GetHashCode(&path[0], &path[0] + path.Size()));
        hashCode.Add(HashCode::GetHashCode(&assemblyPath[0], &assemblyPath[0] + assemblyPath.Size()));
        hashCode.Add(HashCode::GetHashCode(&className[0], &className[0] + className.Size()));
        hashCode.Add(compileStatus);

        return hashCode;
    }
};

static_assert(std::is_standard_layout_v<ScriptData>, "ScriptData struct must be standard layout");
static_assert(std::is_trivially_copyable_v<ScriptData>, "ScriptData struct must be a trivial type");

} // namespace hyperion
