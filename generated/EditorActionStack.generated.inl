#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region EditorActionStack Reflection Data

HYP_BEGIN_CLASS(EditorActionStack, 36, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(Push)), &EditorActionStack::Push),
    HypMethod(NAME(HYP_STR(CanUndo)), &EditorActionStack::CanUndo),
    HypMethod(NAME(HYP_STR(CanRedo)), &EditorActionStack::CanRedo),
    HypMethod(NAME(HYP_STR(Undo)), &EditorActionStack::Undo),
    HypMethod(NAME(HYP_STR(Redo)), &EditorActionStack::Redo),
    HypMethod(NAME(HYP_STR(GetUndoAction)), &EditorActionStack::GetUndoAction),
    HypMethod(NAME(HYP_STR(GetRedoAction)), &EditorActionStack::GetRedoAction),
    HypField(NAME(HYP_STR(OnBeforeActionPush)), &EditorActionStack::OnBeforeActionPush, offsetof(EditorActionStack, OnBeforeActionPush), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnBeforeActionPop)), &EditorActionStack::OnBeforeActionPop, offsetof(EditorActionStack, OnBeforeActionPop), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnAfterActionPush)), &EditorActionStack::OnAfterActionPush, offsetof(EditorActionStack, OnAfterActionPush), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnAfterActionPop)), &EditorActionStack::OnAfterActionPop, offsetof(EditorActionStack, OnAfterActionPop), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnStateChange)), &EditorActionStack::OnStateChange, offsetof(EditorActionStack, OnStateChange), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } })
HYP_END_CLASS

#pragma endregion EditorActionStack Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region EditorActionStackState Reflection Data

HYP_BEGIN_ENUM(EditorActionStackState, 255, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), EditorActionStackState::NONE),
    HypConstant(NAME(HYP_STR(CAN_UNDO)), EditorActionStackState::CAN_UNDO),
    HypConstant(NAME(HYP_STR(CAN_REDO)), EditorActionStackState::CAN_REDO)
HYP_END_ENUM

#pragma endregion EditorActionStackState Reflection Data

} // namespace hyperion

