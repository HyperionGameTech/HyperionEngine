/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <dotnet/ManagedObject.hpp>

#include <EditorActionStack.generated.inl>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

EditorActionStack::EditorActionStack()
    : m_undoDepth(-1),
      m_currentState(EditorActionStackState::NONE)
{
}

EditorActionStack::EditorActionStack(const WeakHandle<EditorProject>& editorProject)
    : m_editorProject(editorProject),
      m_undoDepth(-1),
      m_currentState(EditorActionStackState::NONE)
{
}

EditorActionStack::EditorActionStack(EditorActionStack&& other) noexcept
    : m_editorProject(std::move(other.m_editorProject)),
      m_actions(std::move(other.m_actions)),
      m_undoDepth(other.m_undoDepth),
      m_currentState(other.m_currentState)
{
    other.m_undoDepth = -1;
    other.m_currentState = EditorActionStackState::NONE;
}

EditorActionStack& EditorActionStack::operator=(EditorActionStack&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_editorProject = std::move(other.m_editorProject);

    m_actions = std::move(other.m_actions);
    m_undoDepth = other.m_undoDepth;
    m_currentState = other.m_currentState;

    other.m_undoDepth = -1;
    other.m_currentState = EditorActionStackState::NONE;

    return *this;
}

EditorActionStack::~EditorActionStack() = default;

bool EditorActionStack::CanUndo() const
{
    return m_undoDepth >= 0;
}

bool EditorActionStack::CanRedo() const
{
    return m_undoDepth + 1 < m_actions.Size();
}

bool EditorActionStack::PushAction(const Handle<EditorActionBase>& action)
{
    Assert(action.IsValid());

    Handle<EditorProject> editorProject = m_editorProject.Lock();
    Assert(editorProject.IsValid());

    Handle<EditorSubsystem> editorSubsystem = editorProject->GetEditorSubsystem().Lock();
    Assert(editorSubsystem.IsValid());

    const EnumFlags<EditorActionStackState> previousState = m_currentState;

    OnBeforeActionPush(action.Get());

    action->Execute(editorSubsystem.Get(), editorProject.Get());

    // Chop off any actions stack that are after the current action index,
    // since we are pushing a new action.
    if (m_undoDepth + 1 < m_actions.Size())
    {
        auto it = m_actions.Begin() + m_undoDepth + 1;
        while (it != m_actions.End())
        {
            it = m_actions.Erase(it);
        }
    }

    m_actions.PushBack(action);

    UpdateState(m_undoDepth + 1);

    OnAfterActionPush(action.Get());

    return true;
}

void EditorActionStack::Undo()
{
    if (!CanUndo())
    {
        return;
    }

    Handle<EditorProject> editorProject = m_editorProject.Lock();
    Assert(editorProject.IsValid());

    Handle<EditorSubsystem> editorSubsystem = editorProject->GetEditorSubsystem().Lock();
    Assert(editorSubsystem.IsValid());

    EditorActionBase* action = m_actions[m_undoDepth].Get();
    HYP_LOG(Editor, Verbose, "Undoing action: {}", action->GetText());

    OnBeforeActionPop(action);

    action->Revert(editorSubsystem.Get(), editorProject.Get());

    UpdateState(m_undoDepth - 1);

    OnAfterActionPop(action);
}

void EditorActionStack::Redo()
{
    if (!CanRedo())
    {
        return;
    }

    Handle<EditorProject> editorProject = m_editorProject.Lock();
    Assert(editorProject.IsValid());

    Handle<EditorSubsystem> editorSubsystem = editorProject->GetEditorSubsystem().Lock();
    Assert(editorSubsystem.IsValid());

    EditorActionBase* action = m_actions[m_undoDepth + 1].Get();
    HYP_LOG(Editor, Verbose, "Redoing action: {}", action->GetText());

    OnBeforeActionPush(action);

    action->Execute(editorSubsystem.Get(), editorProject.Get());

    UpdateState(m_undoDepth + 1);

    OnAfterActionPush(action);
}

const Handle<EditorActionBase>& EditorActionStack::GetUndoAction() const
{
    if (!CanUndo())
    {
        return Handle<EditorActionBase>::empty;
    }

    AssertDebug(m_undoDepth < m_actions.Size());

    const Handle<EditorActionBase>& action = m_actions[m_undoDepth];
    AssertDebug(action.IsValid());

    return action;
}

const Handle<EditorActionBase>& EditorActionStack::GetRedoAction() const
{
    if (!CanRedo())
    {
        return Handle<EditorActionBase>::empty;
    }

    AssertDebug(m_undoDepth + 1 < m_actions.Size());

    const Handle<EditorActionBase>& action = m_actions[m_undoDepth + 1];
    AssertDebug(action.IsValid());

    return action;
}

void EditorActionStack::UpdateState(int newUndoDepth)
{
    EnumFlags<EditorActionStackState> newState = EditorActionStackState::NONE;
    newState[EditorActionStackState::CAN_UNDO] = CanUndo();
    newState[EditorActionStackState::CAN_REDO] = CanRedo();

    if (m_currentState != newState || newUndoDepth != m_undoDepth)
    {
        m_currentState = newState;
        m_undoDepth = newUndoDepth;

        OnStateChange(m_currentState, m_undoDepth);
    }
}

} // namespace Hyperion
