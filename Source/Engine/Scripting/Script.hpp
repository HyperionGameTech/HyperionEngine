#pragma once

#include <Scripting/ScriptFwd.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/Uuid.hpp>

#include <Core/Memory/SharedPtr.hpp>
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

    HYP_FIELD(Property = "UUID", Serialize)
    UUID uuid;

    HYP_FIELD(Property = "Language", Serialize)
    ScriptLanguage language;

    HYP_FIELD(Transient)
    FixedArray<char, ScriptMaxPathLength> path;             // backing field; serialized with getter/setter

    HYP_FIELD(Transient)
    FixedArray<char, ScriptMaxPathLength> assemblyPath;     // backing field; serialized with getter/setter - C# only

    HYP_FIELD(Transient)
    FixedArray<char, ScriptMaxClassNameLength> className;   // backing field; serialized with getter/setter

    HYP_FIELD(Transient)
    uint8 compileStatus;

    HYP_FIELD(Transient)
    int32 hotReloadVersion;

    HYP_FIELD(Transient)
    uint64 lastModifiedTimestamp;

    ScriptDesc()
        : language(ScriptLanguage::HypScript),
          path{},
          assemblyPath{},
          className{},
          compileStatus(0),
          hotReloadVersion(0),
          lastModifiedTimestamp(0)
    {
    }

    // Serialization helpers

    // Path
    HYP_METHOD(Property = "Path", Serialize, NoScriptBindings)
    String SerializePath() const
    {
        return String(path.Data());
    }

    HYP_METHOD(Property = "Path", Serialize, NoScriptBindings)
    void DeserializePath(const String& path)
    {
        const size_t maxSize = path.Size() <= this->path.Size() - 1
            ? path.Size() : this->path.Size() - 1;

        Memory::Copy(this->path.Data(), path.Data(), maxSize);
        this->path[maxSize] = '\0';
    }

    // AssemblyPath

    HYP_METHOD(Property = "AssemblyPath", Serialize, NoScriptBindings)
    String SerializeAssemblyPath() const
    {
        return String(assemblyPath.Data());
    }

    HYP_METHOD(Property = "AssemblyPath", Serialize, NoScriptBindings)
    void DeserializeAssemblyPath(const String& assemblyPath)
    {
        const size_t maxSize = assemblyPath.Size() <= this->assemblyPath.Size() - 1
            ? assemblyPath.Size() : this->assemblyPath.Size() - 1;

        Memory::Copy(this->assemblyPath.Data(), assemblyPath.Data(), maxSize);
        this->assemblyPath[maxSize] = '\0';
    }

    // className

    HYP_METHOD(Property = "ClassName", Serialize, NoScriptBindings)
    String SerializeClassName() const
    {
        return String(className.Data());
    }

    HYP_METHOD(Property = "ClassName", Serialize, NoScriptBindings)
    void DeserializeClassName(const String& className)
    {
        const size_t maxSize = className.Size() <= this->className.Size() - 1
            ? className.Size() : this->className.Size() - 1;

        Memory::Copy(this->className.Data(), className.Data(), maxSize);
        this->className[maxSize] = '\0';
    }

    /// =====
};

static_assert(std::is_standard_layout_v<ScriptDesc>, "ScriptDesc struct must be standard layout");
static_assert(std::is_trivially_copyable_v<ScriptDesc>, "ScriptDesc struct must be a trivial type");

} // namespace Hyperion
