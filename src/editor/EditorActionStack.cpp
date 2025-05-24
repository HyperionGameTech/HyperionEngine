/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorActionStack.hpp>

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

EditorActionStack::EditorActionStack(EditorActionStack &&other) noexcept
    : m_actions(Move(other.m_actions)),
      m_current_action_index(other.m_current_action_index),
      m_current_state(other.m_current_state)
{
    other.m_current_action_index = -1;
    other.m_current_state = EditorActionStackState::NONE;
}

EditorActionStack &EditorActionStack::operator=(EditorActionStack &&other) noexcept
{
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

void EditorActionStack::Push(const RC<IEditorAction> &action)
{
    AssertThrow(action != nullptr);

    const EnumFlags<EditorActionStackState> previous_state = m_current_state;

    OnBeforeActionPush(action.Get());

    action->Execute();

    // Chop off any actions that were undone
    if (m_current_action_index + 1 < m_actions.Size()) {
        m_actions.Resize(m_current_action_index + 1);
    }

    m_actions.PushBack(action);
    m_current_action_index = int(m_actions.Size()) - 1;

    UpdateState();

    OnAfterActionPush(action.Get());
}

void EditorActionStack::Undo()
{
    if (!CanUndo()) {
        return;
    }

    IEditorAction *action = m_actions[m_current_action_index].Get();

    OnBeforeActionPop(action);

    action->Revert();
    
    --m_current_action_index;

    UpdateState();

    OnAfterActionPop(action);
}

void EditorActionStack::Redo()
{
    if (!CanRedo()) {
        return;
    }

    IEditorAction *action = m_actions[m_current_action_index + 1].Get();

    OnBeforeActionPush(action);

    action->Execute();

    ++m_current_action_index;

    UpdateState();

    OnAfterActionPush(action);
}

IEditorAction *EditorActionStack::GetUndoAction() const
{
    if (!CanUndo()) {
        return nullptr;
    }

    return m_actions[m_current_action_index].Get();
}

IEditorAction *EditorActionStack::GetRedoAction() const
{
    if (!CanRedo()) {
        return nullptr;
    }

    return m_actions[m_current_action_index + 1].Get();
}

void EditorActionStack::UpdateState()
{
    EnumFlags<EditorActionStackState> new_state = EditorActionStackState::NONE;
    new_state[EditorActionStackState::CAN_UNDO] = CanUndo();
    new_state[EditorActionStackState::CAN_REDO] = CanRedo();

    if (m_current_state != new_state) {
        m_current_state = new_state;

        OnStateChange(m_current_state);
    }
}

} // namespace hyperion