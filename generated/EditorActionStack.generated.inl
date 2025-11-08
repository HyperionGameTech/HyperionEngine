#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region EditorActionStack Reflection Data

HYP_BEGIN_CLASS(EditorActionStack, 36, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(Push)), &EditorActionStack::Push),
    Method(NAME(HYP_STR(CanUndo)), &EditorActionStack::CanUndo),
    Method(NAME(HYP_STR(CanRedo)), &EditorActionStack::CanRedo),
    Method(NAME(HYP_STR(Undo)), &EditorActionStack::Undo),
    Method(NAME(HYP_STR(Redo)), &EditorActionStack::Redo),
    Method(NAME(HYP_STR(GetUndoAction)), &EditorActionStack::GetUndoAction),
    Method(NAME(HYP_STR(GetRedoAction)), &EditorActionStack::GetRedoAction),
    Field(NAME(HYP_STR(OnBeforeActionPush)), &EditorActionStack::OnBeforeActionPush, offsetof(EditorActionStack, OnBeforeActionPush), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnBeforeActionPop)), &EditorActionStack::OnBeforeActionPop, offsetof(EditorActionStack, OnBeforeActionPop), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnAfterActionPush)), &EditorActionStack::OnAfterActionPush, offsetof(EditorActionStack, OnAfterActionPush), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnAfterActionPop)), &EditorActionStack::OnAfterActionPop, offsetof(EditorActionStack, OnAfterActionPop), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnStateChange)), &EditorActionStack::OnStateChange, offsetof(EditorActionStack, OnStateChange), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } })
HYP_END_CLASS

#pragma endregion EditorActionStack Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region EditorActionStackState Reflection Data

HYP_BEGIN_ENUM(EditorActionStackState, 257, 0, {})
    StaticField(NAME(HYP_STR(NONE)), EditorActionStackState::NONE),
    StaticField(NAME(HYP_STR(CAN_UNDO)), EditorActionStackState::CAN_UNDO),
    StaticField(NAME(HYP_STR(CAN_REDO)), EditorActionStackState::CAN_REDO)
HYP_END_ENUM

#pragma endregion EditorActionStackState Reflection Data

} // namespace hyperion

