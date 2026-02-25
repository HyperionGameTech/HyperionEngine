/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/EditorActionStack.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <dotnet/ManagedObject.hpp>

#include <EditorActionStack.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditorActionStack::EditorActionStack()
    : m_currentActionIndex(-1),
      m_currentState(EditorActionStackState::NONE)
{
}

EditorActionStack::EditorActionStack(const WeakHandle<EditorProject>& editorProject)
    : m_editorProject(editorProject),
      m_currentActionIndex(-1),
      m_currentState(EditorActionStackState::NONE)
{
}

EditorActionStack::EditorActionStack(EditorActionStack&& other) noexcept
    : m_editorProject(std::move(other.m_editorProject)),
      m_actions(std::move(other.m_actions)),
      m_currentActionIndex(other.m_currentActionIndex),
      m_currentState(other.m_currentState)
{
    other.m_currentActionIndex = -1;
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
    m_currentActionIndex = other.m_currentActionIndex;
    m_currentState = other.m_currentState;

    other.m_currentActionIndex = -1;
    other.m_currentState = EditorActionStackState::NONE;

    return *this;
}

EditorActionStack::~EditorActionStack() = default;

bool EditorActionStack::CanUndo() const
{
    return m_currentActionIndex >= 0;
}

bool EditorActionStack::CanRedo() const
{
    return m_currentActionIndex + 1 < m_actions.Size();
}

void EditorActionStack::Push(const Handle<EditorActionBase>& action)
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
    if (m_currentActionIndex > 0)
    {
        while (int(m_actions.Size()) > m_currentActionIndex)
        {
            m_actions.PopBack();
        }
    }

    m_actions.PushBack(action);
    m_currentActionIndex = int(m_actions.Size()) - 1;

    UpdateState();

    OnAfterActionPush(action.Get());
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

    EditorActionBase* action = m_actions[m_currentActionIndex].Get();

    OnBeforeActionPop(action);

    action->Revert(editorSubsystem.Get(), editorProject.Get());

    --m_currentActionIndex;

    UpdateState();

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

    EditorActionBase* action = m_actions[m_currentActionIndex + 1].Get();

    OnBeforeActionPush(action);

    action->Execute(editorSubsystem.Get(), editorProject.Get());

    ++m_currentActionIndex;

    UpdateState();

    OnAfterActionPush(action);
}

const Handle<EditorActionBase>& EditorActionStack::GetUndoAction() const
{
    if (!CanUndo())
    {
        return Handle<EditorActionBase>::empty;
    }

    AssertDebug(m_currentActionIndex < m_actions.Size());

    const Handle<EditorActionBase>& action = m_actions[m_currentActionIndex];
    AssertDebug(action.IsValid());

    return action;
}

const Handle<EditorActionBase>& EditorActionStack::GetRedoAction() const
{
    if (!CanRedo())
    {
        return Handle<EditorActionBase>::empty;
    }

    AssertDebug(m_currentActionIndex + 1 < m_actions.Size());

    const Handle<EditorActionBase>& action = m_actions[m_currentActionIndex + 1];
    AssertDebug(action.IsValid());

    return action;
}

void EditorActionStack::UpdateState()
{
    EnumFlags<EditorActionStackState> newState = EditorActionStackState::NONE;
    newState[EditorActionStackState::CAN_UNDO] = CanUndo();
    newState[EditorActionStackState::CAN_REDO] = CanRedo();

    if (m_currentState != newState)
    {
        m_currentState = newState;

        OnStateChange(m_currentState);
    }
}

} // namespace Hyperion
