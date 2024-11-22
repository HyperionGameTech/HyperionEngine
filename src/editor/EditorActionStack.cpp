/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorActionStack.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Object.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditorActionStack::EditorActionStack()
    : m_current_action_index(0)
{
}

EditorActionStack::EditorActionStack(EditorActionStack &&other) noexcept
    : m_actions(Move(other.m_actions)),
      m_current_action_index(other.m_current_action_index)
{
    other.m_current_action_index = 0;
}

EditorActionStack &EditorActionStack::operator=(EditorActionStack &&other) noexcept
{
    m_actions = Move(other.m_actions);
    m_current_action_index = other.m_current_action_index;

    other.m_current_action_index = 0;

    return *this;
}

EditorActionStack::~EditorActionStack() = default;

bool EditorActionStack::CanUndo() const
{
    return m_current_action_index > 0;
}

bool EditorActionStack::CanRedo() const
{
    return m_current_action_index < m_actions.Size();
}

void EditorActionStack::Push(const RC<IEditorAction> &action)
{
    AssertThrow(action != nullptr);

    OnBeforeActionPush(action.Get());

    action->Execute();

    if (m_current_action_index < m_actions.Size()) {
        m_actions.Resize(m_current_action_index);
    }

    m_actions.PushBack(action);

    OnAfterActionPush(action.Get());
}

void EditorActionStack::Undo()
{
    if (!CanUndo()) {
        return;
    }

    IEditorAction *action = m_actions[m_current_action_index - 1].Get();

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

    IEditorAction *action = m_actions[m_current_action_index].Get();

    OnBeforeActionPush(action);

    action->Execute();

    ++m_current_action_index;

    OnAfterActionPush(action);
}

} // namespace hyperion