/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_ACTION_STACK_HPP
#define HYPERION_EDITOR_ACTION_STACK_HPP

#include <editor/EditorAction.hpp>

#include <core/containers/Array.hpp>

#include <core/functional/Delegate.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/object/HypObject.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API EditorActionStack : public EnableRefCountedPtrFromThis<EditorActionStack>
{
    HYP_OBJECT_BODY(EditorActionStack);

public:
    EditorActionStack();
    EditorActionStack(const EditorActionStack &other)               = delete;
    EditorActionStack &operator=(const EditorActionStack &other)    = delete;
    EditorActionStack(EditorActionStack &&other) noexcept;
    EditorActionStack &operator=(EditorActionStack &&other) noexcept;
    ~EditorActionStack();

    HYP_METHOD()
    void Push(const RC<IEditorAction> &action);

    HYP_METHOD()
    bool CanUndo() const;

    HYP_METHOD()
    bool CanRedo() const;

    HYP_METHOD()
    void Undo();

    HYP_METHOD()
    void Redo();

    Delegate<void, IEditorAction *> OnBeforeActionPush;
    Delegate<void, IEditorAction *> OnBeforeActionPop;
    Delegate<void, IEditorAction *> OnAfterActionPush;
    Delegate<void, IEditorAction *> OnAfterActionPop;

private:
    Array<RC<IEditorAction>>    m_actions;
    int                         m_current_action_index;
};

} // namespace hyperion

#endif

