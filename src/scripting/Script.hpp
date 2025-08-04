#pragma once
#include <core/containers/String.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Uuid.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/object/HypObject.hpp>

#include <core/filesystem/FilePath.hpp>

#include <HashCode.hpp>

namespace hyperion {

namespace dotnet {
class Assembly;
class Object;
} // namespace dotnet

enum class CompiledScriptState : uint32
{
    UNINITIALIZED = 0x0,
    COMPILED = 0x1,
    DIRTY = 0x2,
    PROCESSING = 0x4,
    ERRORED = 0x8
};

HYP_MAKE_ENUM_FLAGS(CompiledScriptState)

struct ScriptDesc
{
    FilePath path;
};

static constexpr SizeType scriptMaxPathLength = 1024;
static constexpr SizeType scriptMaxClassNameLength = 1024;

HYP_STRUCT()

struct ManagedScript
{
    HYP_FIELD(Serialize, Property = "UUID")
    UUID uuid;

    HYP_FIELD(Serialize, Property = "Path")
    char path[scriptMaxPathLength];

    HYP_FIELD(Serialize, Property = "AssemblyPath")
    char assemblyPath[scriptMaxPathLength];

    HYP_FIELD(Serialize, Property = "ClassName")
    char className[scriptMaxClassNameLength];

    HYP_FIELD(Serialize, Property = "State")
    uint32 state;

    HYP_FIELD(Serialize, Property = "HotReloadVersion")
    int32 hotReloadVersion;

    HYP_FIELD(Serialize, Property = "LastModifiedTimestamp")
    uint64 lastModifiedTimestamp;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(uuid);
        hashCode.Add(&path[0]);
        hashCode.Add(&assemblyPath[0]);
        hashCode.Add(&className[0]);
        hashCode.Add(state);

        return hashCode;
    }
};

static_assert(std::is_standard_layout_v<ManagedScript>, "ManagedScript struct must be standard layout");
static_assert(std::is_trivially_copyable_v<ManagedScript>, "ManagedScript struct must be a trivial type");
static_assert(sizeof(ManagedScript) == 3104, "ManagedScript struct size must match C# struct size");

HYP_CLASS()
class HYP_API Script : public HypObjectBase
{
    HYP_OBJECT_BODY(Script);

public:
    Script();
    Script(const ScriptDesc& desc);
    Script(const Script& other) = delete;
    Script& operator=(const Script& other) = delete;
    Script(Script&& other) noexcept = delete;
    Script& operator=(Script&& other) noexcept = delete;
    ~Script();

    HYP_FORCE_INLINE const ScriptDesc& GetDescriptor() const
    {
        return m_desc;
    }

    HYP_FORCE_INLINE ManagedScript& GetManagedScript()
    {
        return m_managedScript;
    }

    HYP_FORCE_INLINE const ManagedScript& GetManagedScript() const
    {
        return m_managedScript;
    }

    HYP_FORCE_INLINE void SetManagedScript(const ManagedScript& managedScript)
    {
        m_managedScript = managedScript;
    }

    EnumFlags<CompiledScriptState> GetState() const;

private:
    ScriptDesc m_desc;
    ManagedScript m_managedScript;
};

} // namespace hyperion

