/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorActionStack.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Object.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditorActionStack::EditorActionStack()
    : m_current_action_index(-1),
      m_current_state(EditorActionStackState::NONE)
{
}

EditorActionStack::EditorActionStack(const WeakHandle<EditorProject>& editor_project)
    : m_editor_project(editor_project),
      m_current_action_index(-1),
      m_current_state(EditorActionStackState::NONE)
{
}

EditorActionStack::EditorActionStack(EditorActionStack&& other) noexcept
    : m_editor_project(std::move(other.m_editor_project)),
      m_actions(Move(other.m_actions)),
      m_current_action_index(other.m_current_action_index),
      m_current_state(other.m_current_state)
{
    other.m_current_action_index = -1;
    other.m_current_state = EditorActionStackState::NONE;
}

EditorActionStack& EditorActionStack::operator=(EditorActionStack&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_editor_project = std::move(other.m_editor_project);

    m_actions = Move(other.m_actions);
    m_current_action_index = other.m_current_action_index;
    m_current_state = other.m_current_state;

    other.m_current_action_index = -1;
    other.m_current_state = EditorActionStackState::NONE;

    return *this;
}

EditorActionStack::~EditorActionStack() = default;

bool EditorActionStack::CanUndo() const
{
    return m_current_action_index >= 0;
}

bool EditorActionStack::CanRedo() const
{
    return m_current_action_index + 1 < m_actions.Size();
}

void EditorActionStack::Push(const Handle<EditorActionBase>& action)
{
    AssertThrow(action.IsValid());

    Handle<EditorProject> editor_project = m_editor_project.Lock();
    AssertThrow(editor_project.IsValid());

    Handle<EditorSubsystem> editor_subsystem = editor_project->GetEditorSubsystem().Lock();
    AssertThrow(editor_subsystem.IsValid());

    const EnumFlags<EditorActionStackState> previous_state = m_current_state;

    OnBeforeActionPush(action.Get());

    action->Execute(editor_subsystem.Get(), editor_project.Get());

    // Chop off any actions stack that are after the current action index,
    // since we are pushing a new action.
    if (m_current_action_index > 0)
    {
        while (int(m_actions.Size()) > m_current_action_index)
        {
            m_actions.PopBack();
        }
    }

    m_actions.PushBack(action);
    m_current_action_index = int(m_actions.Size()) - 1;

    UpdateState();

    OnAfterActionPush(action.Get());
}

void EditorActionStack::Undo()
{
    if (!CanUndo())
    {
        return;
    }

    Handle<EditorProject> editor_project = m_editor_project.Lock();
    AssertThrow(editor_project.IsValid());

    Handle<EditorSubsystem> editor_subsystem = editor_project->GetEditorSubsystem().Lock();
    AssertThrow(editor_subsystem.IsValid());

    EditorActionBase* action = m_actions[m_current_action_index].Get();

    OnBeforeActionPop(action);

    action->Revert(editor_subsystem.Get(), editor_project.Get());

    --m_current_action_index;

    UpdateState();

    OnAfterActionPop(action);
}

void EditorActionStack::Redo()
{
    if (!CanRedo())
    {
        return;
    }

    Handle<EditorProject> editor_project = m_editor_project.Lock();
    AssertThrow(editor_project.IsValid());

    Handle<EditorSubsystem> editor_subsystem = editor_project->GetEditorSubsystem().Lock();
    AssertThrow(editor_subsystem.IsValid());

    EditorActionBase* action = m_actions[m_current_action_index + 1].Get();

    OnBeforeActionPush(action);

    action->Execute(editor_subsystem.Get(), editor_project.Get());

    ++m_current_action_index;

    UpdateState();

    OnAfterActionPush(action);
}

const Handle<EditorActionBase>& EditorActionStack::GetUndoAction() const
{
    if (!CanUndo())
    {
        return Handle<EditorActionBase>::empty;
    }

    AssertDebug(m_current_action_index < m_actions.Size());

    const Handle<EditorActionBase>& action = m_actions[m_current_action_index];
    AssertDebug(action.IsValid());

    return action;
}

const Handle<EditorActionBase>& EditorActionStack::GetRedoAction() const
{
    if (!CanRedo())
    {
        return Handle<EditorActionBase>::empty;
    }

    AssertDebug(m_current_action_index + 1 < m_actions.Size());

    const Handle<EditorActionBase>& action = m_actions[m_current_action_index + 1];
    AssertDebug(action.IsValid());

    return action;
}

void EditorActionStack::UpdateState()
{
    EnumFlags<EditorActionStackState> new_state = EditorActionStackState::NONE;
    new_state[EditorActionStackState::CAN_UNDO] = CanUndo();
    new_state[EditorActionStackState::CAN_REDO] = CanRedo();

    if (m_current_state != new_state)
    {
        m_current_state = new_state;

        OnStateChange(m_current_state);
    }
}

} // namespace hyperion