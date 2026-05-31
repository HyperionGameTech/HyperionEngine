#pragma once

#include <Scripting/ScriptFwd.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/Uuid.hpp>

#include <Core/Memory/RefCountedPtr.hpp>
#include <Core/Memory/UniquePtr.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/FileSystem/FilePath.hpp>

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
