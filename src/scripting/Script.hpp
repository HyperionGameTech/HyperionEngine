#ifndef HYPERION_SCRIPT_HPP
#define HYPERION_SCRIPT_HPP

#include <core/containers/String.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/filesystem/FilePath.hpp>

namespace hyperion {

namespace dotnet {
class Assembly;
class Object;
} // namespace dotnet

enum class CompiledScriptState : uint32
{
    UNINITIALIZED   = 0x0,
    COMPILED        = 0x1,
    DIRTY           = 0x2,
    PROCESSING      = 0x4,
    ERRORED         = 0x8
};

HYP_MAKE_ENUM_FLAGS(CompiledScriptState)

struct CompiledScript
{
    RC<dotnet::Assembly>            assembly;
    UniquePtr<dotnet::Object>       object;
};

struct ScriptDesc
{
    FilePath    path;
};


static constexpr SizeType script_max_path_length = 1024;

struct ManagedScript
{
    char    path[script_max_path_length];
    char    assembly_path[script_max_path_length];
    uint32  state;
    uint64  last_modified_timestamp;
};

static_assert(std::is_standard_layout_v<ManagedScript>, "ManagedScript struct must be standard layout");
static_assert(std::is_trivial_v<ManagedScript>, "ManagedScript struct must be a trivial type");
static_assert(sizeof(ManagedScript) == 2064, "ManagedScript struct size must match C# struct size");

class HYP_API Script
{
public:
    Script(const ScriptDesc &desc);
    Script(const Script &other)                 = delete;
    Script &operator=(const Script &other)      = delete;
    Script(Script &&other) noexcept             = delete;
    Script &operator=(Script &&other) noexcept  = delete;
    ~Script();

    [[nodiscard]]
    HYP_FORCE_INLINE
    const ScriptDesc &GetDescriptor() const
        { return m_desc; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const CompiledScript &GetCompiledScript() const
        { return m_compiled_script; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    ManagedScript &GetManagedScript()
        { return m_managed_script; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const ManagedScript &GetManagedScript() const
        { return m_managed_script; }

    [[nodiscard]] EnumFlags<CompiledScriptState> GetState() const;

private:
    ScriptDesc          m_desc;
    CompiledScript      m_compiled_script;
    ManagedScript       m_managed_script;
};

} // namespace hyperion

#endif