/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorActionStack.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace editor {

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

void EditorActionStack::Push(UniquePtr<IEditorAction> &&action)
{
    if (m_current_action_index < m_actions.Size()) {
        m_actions.Resize(m_current_action_index);
    }

    m_actions.PushBack(Move(action));
}

void EditorActionStack::Undo()
{
    if (!CanUndo()) {
        return;
    }

    m_actions[--m_current_action_index]->Undo();
}

void EditorActionStack::Redo()
{
    if (!CanRedo()) {
        return;
    }

    m_actions[m_current_action_index++]->Redo();
}

} // namespace editor
} // namespace hyperion