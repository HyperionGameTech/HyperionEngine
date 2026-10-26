/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <PluginTestEditor/PluginTestEditorExtension.hpp>

#include <Editor/Commands/EditorCommandsCommon.hpp>

#include <PluginTestEditorExtension.generated.inl>

namespace Hyperion {

void PluginTestEditorExtension::OnAttached()
{
    HYP_LOG(Editor, Info, "PluginTestEditorExtension attached");
}

void PluginTestEditorExtension::OnDetached()
{
    HYP_LOG(Editor, Info, "PluginTestEditorExtension detached after {} update(s)", m_numUpdates);
}

void PluginTestEditorExtension::Update(float delta)
{
    ++m_numUpdates;
}

} // namespace Hyperion
