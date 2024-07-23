/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_ACTION_STACK_HPP
#define HYPERION_EDITOR_ACTION_STACK_HPP

#include <editor/EditorAction.hpp>

#include <core/containers/Array.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace editor {

class HYP_API EditorActionStack
{
public:
    EditorActionStack();
    EditorActionStack(const EditorActionStack &other)               = delete;
    EditorActionStack &operator=(const EditorActionStack &other)    = delete;
    EditorActionStack(EditorActionStack &&other) noexcept;
    EditorActionStack &operator=(EditorActionStack &&other) noexcept;
    ~EditorActionStack();

    void Push(UniquePtr<IEditorAction> &&action);

    bool CanUndo() const;
    bool CanRedo() const;

    void Undo();
    void Redo();

private:
    Array<UniquePtr<IEditorAction>> m_actions;
    int                             m_current_action_index;
};

} // namespace editor
} // namespace hyperion

#endif

