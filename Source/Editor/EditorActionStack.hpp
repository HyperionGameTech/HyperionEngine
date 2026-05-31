/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Editor/EditorAction.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/LinkedList.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

HYP_ENUM()
enum class EditorActionStackState : uint32
{
    NONE = 0x0,
    CAN_UNDO = 0x1,
    CAN_REDO = 0x2
};

HYP_MAKE_ENUM_FLAGS(EditorActionStackState)

HYP_CLASS()
class EDITOR_API EditorActionStack : public ObjectBase
{
    HYP_OBJECT_BODY(EditorActionStack);

public:
    EditorActionStack();

    EditorActionStack(const WeakHandle<EditorProject>& editorProject);

    EditorActionStack(const EditorActionStack& other) = delete;
    EditorActionStack& operator=(const EditorActionStack& other) = delete;

    EditorActionStack(EditorActionStack&& other) noexcept;
    EditorActionStack& operator=(EditorActionStack&& other) noexcept;

    virtual ~EditorActionStack() override;

    HYP_METHOD()
    bool PushAction(const Handle<EditorActionBase>& action);

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
    ScriptableDelegate<void, EnumFlags<EditorActionStackState> /* state */, int /* undoDepth */> OnStateChange;

private:
    void UpdateState(int newUndoDepth);

    WeakHandle<EditorProject> m_editorProject;

    Array<Handle<EditorActionBase>> m_actions;
    int m_undoDepth;

    EnumFlags<EditorActionStackState> m_currentState;
};

} // namespace Hyperion
