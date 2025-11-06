#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region EditorState Reflection Data

HYP_BEGIN_CLASS(EditorState, 38, 0, NAME("HypObjectBase"))
    Method(NAME(HYP_STR(GetCurrentProject)), &EditorState::GetCurrentProject),
    Method(NAME(HYP_STR(SetCurrentProject)), &EditorState::SetCurrentProject),
    Field(NAME(HYP_STR(OnCurrentProjectChanged)), &EditorState::OnCurrentProjectChanged, offsetof(EditorState, OnCurrentProjectChanged), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } })
HYP_END_CLASS

#pragma endregion EditorState Reflection Data

} // namespace hyperion

