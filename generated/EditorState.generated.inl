#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region EditorState Reflection Data

HYP_BEGIN_CLASS(EditorState, 197, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetCurrentProject)), &EditorState::GetCurrentProject),
    HypMethod(NAME(HYP_STR(SetCurrentProject)), &EditorState::SetCurrentProject),
    HypField(NAME(HYP_STR(OnCurrentProjectChanged)), &EditorState::OnCurrentProjectChanged, offsetof(EditorState, OnCurrentProjectChanged), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } })
HYP_END_CLASS

#pragma endregion EditorState Reflection Data

} // namespace hyperion

