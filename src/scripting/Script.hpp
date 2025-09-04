#pragma once

#include <scripting/ScriptFwd.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Uuid.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/object/HypObject.hpp>

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
    HYP_FIELD(Serialize)
    UUID uuid;

    HYP_FIELD(Serialize)
    ScriptLanguage language = SL_HYPSCRIPT;

    HYP_FIELD(Serialize)
    char path[scriptMaxPathLength];

    HYP_FIELD(Serialize)
    char assemblyPath[scriptMaxPathLength];

    HYP_FIELD(Serialize)
    char className[scriptMaxClassNameLength];

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
        hashCode.Add(&path[0]);
        hashCode.Add(&assemblyPath[0]);
        hashCode.Add(&className[0]);
        hashCode.Add(compileStatus);

        return hashCode;
    }
};

static_assert(std::is_standard_layout_v<ScriptData>, "ScriptData struct must be standard layout");
static_assert(std::is_trivially_copyable_v<ScriptData>, "ScriptData struct must be a trivial type");

} // namespace hyperion
