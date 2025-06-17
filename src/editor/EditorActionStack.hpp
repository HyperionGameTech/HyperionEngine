/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_ACTION_STACK_HPP
#define HYPERION_EDITOR_ACTION_STACK_HPP

#include <editor/EditorAction.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/object/HypObject.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_ENUM()
enum class EditorActionStackState : uint32
{
    NONE = 0x0,
    CAN_UNDO = 0x1,
    CAN_REDO = 0x2
};

HYP_MAKE_ENUM_FLAGS(EditorActionStackState)

HYP_CLASS()
class HYP_API EditorActionStack : public HypObject<EditorActionStack>
{
    HYP_OBJECT_BODY(EditorActionStack);

public:
    EditorActionStack();
    EditorActionStack(const EditorActionStack& other) = delete;
    EditorActionStack& operator=(const EditorActionStack& other) = delete;
    EditorActionStack(EditorActionStack&& other) noexcept;
    EditorActionStack& operator=(EditorActionStack&& other) noexcept;
    virtual ~EditorActionStack() override;

    HYP_METHOD()
    void Push(const Handle<EditorActionBase>& action);

    HYP_METHOD()
    bool CanUndo() const;

    HYP_METHOD()
    bool CanRedo() const;

    HYP_METHOD()
    void Undo();

    HYP_METHOD()
    void Redo();

    HYP_METHOD()
    const Handle<EditorActionBase>& GetUndoAction() const;

    HYP_METHOD()
    const Handle<EditorActionBase>& GetRedoAction() const;

    HYP_FIELD()
    ScriptableDelegate<void, EditorActionBase*> OnBeforeActionPush;

    HYP_FIELD()
    ScriptableDelegate<void, EditorActionBase*> OnBeforeActionPop;

    HYP_FIELD()
    ScriptableDelegate<void, EditorActionBase*> OnAfterActionPush;

    HYP_FIELD()
    ScriptableDelegate<void, EditorActionBase*> OnAfterActionPop;

    HYP_FIELD()
    ScriptableDelegate<void, EnumFlags<EditorActionStackState>> OnStateChange;

private:
    void UpdateState();

    LinkedList<Handle<EditorActionBase>> m_actions;
    int m_current_action_index;

    EnumFlags<EditorActionStackState> m_current_state;
};

} // namespace hyperion

#endif
