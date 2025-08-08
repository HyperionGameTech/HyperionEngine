#include <scripting/Script.hpp>

#include <core/math/MathUtil.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

Script::Script()
    : Script(ScriptDesc {})
{
}

Script::Script(const ScriptDesc& desc)
    : m_desc(desc)
{
    Assert(m_desc.path.Size() + 1 <= scriptMaxPathLength, "Invalid script path: must be <= %llu characters", scriptMaxPathLength - 1);

    Memory::StrCpy(m_managedScript.path, m_desc.path.Data(), MathUtil::Min(m_desc.path.Size() + 1, scriptMaxPathLength));
    Memory::MemSet(m_managedScript.assemblyPath, '\0', scriptMaxPathLength);
    m_managedScript.compileStatus = SCS_UNINITIALIZED;
}

Script::~Script() = default;

EnumFlags<ScriptCompileStatus> Script::GetCompileStatus() const
{
    return EnumFlags<ScriptCompileStatus>(m_managedScript.compileStatus);
}

} // namespace hyperion