#pragma once

#include <scripting/ScriptFwd.hpp>

#include <Core/containers/String.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/Uuid.hpp>

#include <Core/memory/RefCountedPtr.hpp>
#include <Core/memory/UniquePtr.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

static constexpr SizeType ScriptMaxPathLength = 1024;
static constexpr SizeType ScriptMaxClassNameLength = 1024;

HYP_STRUCT()
struct ScriptDesc
{
    HYP_STRUCT_BODY(ScriptDesc);

    HYP_FIELD(Serialize)
    UUID uuid;

    HYP_FIELD(Serialize)
    ScriptLanguage language = ScriptLanguage::HypScript;

    HYP_FIELD(Serialize)
    FixedArray<char, ScriptMaxPathLength> path;

    HYP_FIELD(Serialize)
    FixedArray<char, ScriptMaxPathLength> assemblyPath; // C# only

    HYP_FIELD(Serialize)
    FixedArray<char, ScriptMaxClassNameLength> className;

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

static_assert(std::is_standard_layout_v<ScriptDesc>, "ScriptDesc struct must be standard layout");
static_assert(std::is_trivially_copyable_v<ScriptDesc>, "ScriptDesc struct must be a trivial type");

} // namespace Hyperion
