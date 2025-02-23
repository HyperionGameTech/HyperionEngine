#include <scripting/Script.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

Script::Script(const ScriptDesc &desc)
    : m_desc(desc)
{
    AssertThrowMsg(m_desc.path.Size() + 1 <= script_max_path_length, "Invalid script path: must be <= %llu characters", script_max_path_length - 1);

    Memory::StrCpy(m_managed_script.path, m_desc.path.Data(), MathUtil::Min(m_desc.path.Size() + 1, script_max_path_length));
    Memory::MemSet(m_managed_script.assembly_path, '\0', script_max_path_length);
    m_managed_script.state = uint32(CompiledScriptState::UNINITIALIZED);
}

Script::~Script() = default;

EnumFlags<CompiledScriptState> Script::GetState() const
{
    return EnumFlags<CompiledScriptState>(m_managed_script.state);
}

} // namespace hyperion