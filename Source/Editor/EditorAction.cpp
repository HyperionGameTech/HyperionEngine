/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorAction.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <EditorAction.generated.inl>

#include <DotNET/ManagedMethodUtil.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

String EditorActionBase::GetText() const
{
    if (Optional<String> result = TryInvokeManagedOverride<String>(this, "GetText"))
    {
        return std::move(*result);
    }

    return GetText_Impl();
}

void EditorActionBase::Execute(EditorSubsystem* editorSubsystem, EditorProject* project)
{
    if (TryInvokeManagedOverrideVoid(this, "Execute", editorSubsystem, project))
    {
        return;
    }

    Execute_Impl(editorSubsystem, project);
}

void EditorActionBase::Revert(EditorSubsystem* editorSubsystem, EditorProject* project)
{
    if (TryInvokeManagedOverrideVoid(this, "Revert", editorSubsystem, project))
    {
        return;
    }

    Revert_Impl(editorSubsystem, project);
}

} // namespace Hyperion
