/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorActionStack.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Object.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditorActionStack::EditorActionStack()
    : m_current_action_index(-1)
{
}

EditorActionStack::EditorActionStack(EditorActionStack &&other) noexcept
    : m_actions(Move(other.m_actions)),
      m_current_action_index(other.m_current_action_index)
{
    other.m_current_action_index = -1;
}

EditorActionStack &EditorActionStack::operator=(EditorActionStack &&other) noexcept
{
    m_actions = Move(other.m_actions);
    m_current_action_index = other.m_current_action_index;

    other.m_current_action_index = -1;

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

    OnBeforeActionPush(action.Get());

    action->Execute();

    // Chop off any actions that were undone
    if (m_current_action_index + 1 < m_actions.Size()) {
        m_actions.Resize(m_current_action_index + 1);
    }

    m_actions.PushBack(action);
    m_current_action_index = int(m_actions.Size()) - 1;

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

    OnAfterActionPush(action);
}

} // namespace hyperion