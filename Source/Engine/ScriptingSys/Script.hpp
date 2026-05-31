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

static constexpr size_t ScriptMaxPathLength = 1024;
static constexpr size_t ScriptMaxClassNameLength = 128;

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

    HYP_FIELD(Transient)
    uint8 compileStatus;

    HYP_FIELD(Transient)
    int32 hotReloadVersion;

    HYP_FIELD()
    uint64 lastModifiedTimestamp;
};

static_assert(std::is_standard_layout_v<ScriptDesc>, "ScriptDesc struct must be standard layout");
static_assert(std::is_trivially_copyable_v<ScriptDesc>, "ScriptDesc struct must be a trivial type");

} // namespace Hyperion
