/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Engine/Plugins/PluginContext.hpp>
#include <Engine/Plugins/PluginManager.hpp>

namespace Hyperion {

PluginContext::PluginContext(
    Name pluginName,
    Name moduleName,
    PluginHostMode hostMode)
    : m_pluginName(pluginName),
      m_moduleName(moduleName),
      m_hostMode(hostMode)
{
}

PluginContext::~PluginContext()
{
    Release();
}

bool PluginContext::IsEditorHost() const
{
    return m_hostMode == PluginHostMode::Editor;
}

void PluginContext::AddCleanup(Proc<void()>&& cleanup)
{
    m_cleanups.PushBack(std::move(cleanup));
}

void PluginContext::Release()
{
    for (size_t index = m_cleanups.Size(); index != 0; index--)
    {
        m_cleanups[index - 1]();
    }

    m_cleanups.Clear();

    for (uint32 worldSystemId : m_worldSystemIds)
    {
        WorldSystemRegistry::GetInstance().Unregister(worldSystemId);
    }

    m_worldSystemIds.Clear();

    for (uint32 processorId : m_modelImportProcessorIds)
    {
        ModelImportProcessorRegistry::GetInstance().Unregister(processorId);
    }

    m_modelImportProcessorIds.Clear();
}

} // namespace Hyperion
