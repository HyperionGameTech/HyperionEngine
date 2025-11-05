#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region UIObject Reflection Data

HYP_BEGIN_CLASS(UIObject, 5, 22, NAME("HypObjectBase"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(GetEntity)), &UIObject::GetEntity),
    Method(NAME(HYP_STR(GetStage)), &UIObject::GetStage),
    Method(NAME(HYP_STR(SetStage)), &UIObject::SetStage),
    Method(NAME(HYP_STR(GetName)), &UIObject::GetName, Span<const ClassAttribute> { {ClassAttribute("property", "Name") } }),
    Method(NAME(HYP_STR(SetName)), &UIObject::SetName, Span<const ClassAttribute> { {ClassAttribute("property", "Name") } }),
    Method(NAME(HYP_STR(GetPosition)), &UIObject::GetPosition, Span<const ClassAttribute> { {ClassAttribute("property", "Position") } }),
    Method(NAME(HYP_STR(SetPosition)), &UIObject::SetPosition, Span<const ClassAttribute> { {ClassAttribute("property", "Position") } }),
    Method(NAME(HYP_STR(GetOffsetPosition)), &UIObject::GetOffsetPosition),
    Method(NAME(HYP_STR(GetAbsolutePosition)), &UIObject::GetAbsolutePosition),
    Method(NAME(HYP_STR(IsPositionAbsolute)), &UIObject::IsPositionAbsolute, Span<const ClassAttribute> { {ClassAttribute("property", "IsPositionAbsolute"), ClassAttribute("xmlattribute", "absolute") } }),
    Method(NAME(HYP_STR(SetIsPositionAbsolute)), &UIObject::SetIsPositionAbsolute, Span<const ClassAttribute> { {ClassAttribute("property", "IsPositionAbsolute"), ClassAttribute("xmlattribute", "absolute") } }),
    Method(NAME(HYP_STR(GetSize)), &UIObject::GetSize, Span<const ClassAttribute> { {ClassAttribute("property", "Size") } }),
    Method(NAME(HYP_STR(SetSize)), &UIObject::SetSize, Span<const ClassAttribute> { {ClassAttribute("property", "Size") } }),
    Method(NAME(HYP_STR(GetInnerSize)), &UIObject::GetInnerSize, Span<const ClassAttribute> { {ClassAttribute("property", "InnerSize") } }),
    Method(NAME(HYP_STR(SetInnerSize)), &UIObject::SetInnerSize, Span<const ClassAttribute> { {ClassAttribute("property", "InnerSize") } }),
    Method(NAME(HYP_STR(GetMaxSize)), &UIObject::GetMaxSize, Span<const ClassAttribute> { {ClassAttribute("property", "MaxSize") } }),
    Method(NAME(HYP_STR(SetMaxSize)), &UIObject::SetMaxSize, Span<const ClassAttribute> { {ClassAttribute("property", "MaxSize") } }),
    Method(NAME(HYP_STR(GetActualSize)), &UIObject::GetActualSize),
    Method(NAME(HYP_STR(GetActualSizeClamped)), &UIObject::GetActualSizeClamped),
    Method(NAME(HYP_STR(GetActualInnerSize)), &UIObject::GetActualInnerSize),
    Method(NAME(HYP_STR(GetScrollOffset)), &UIObject::GetScrollOffset),
    Method(NAME(HYP_STR(SetScrollOffset)), &UIObject::SetScrollOffset),
    Method(NAME(HYP_STR(ScrollToChild)), &UIObject::ScrollToChild),
    Method(NAME(HYP_STR(GetVerticalScrollbarSize)), &UIObject::GetVerticalScrollbarSize),
    Method(NAME(HYP_STR(GetHorizontalScrollbarSize)), &UIObject::GetHorizontalScrollbarSize),
    Method(NAME(HYP_STR(CanScrollOnAxis)), &UIObject::CanScrollOnAxis),
    Method(NAME(HYP_STR(GetComputedDepth)), &UIObject::GetComputedDepth),
    Method(NAME(HYP_STR(GetDepth)), &UIObject::GetDepth, Span<const ClassAttribute> { {ClassAttribute("property", "Depth") } }),
    Method(NAME(HYP_STR(SetDepth)), &UIObject::SetDepth, Span<const ClassAttribute> { {ClassAttribute("property", "Depth") } }),
    Method(NAME(HYP_STR(AcceptsFocus)), &UIObject::AcceptsFocus, Span<const ClassAttribute> { {ClassAttribute("property", "AcceptsFocus") } }),
    Method(NAME(HYP_STR(SetAcceptsFocus)), &UIObject::SetAcceptsFocus, Span<const ClassAttribute> { {ClassAttribute("property", "AcceptsFocus") } }),
    Method(NAME(HYP_STR(NeedsUpdate)), &UIObject::NeedsUpdate),
    Method(NAME(HYP_STR(Focus)), &UIObject::Focus),
    Method(NAME(HYP_STR(Blur)), &UIObject::Blur),
    Method(NAME(HYP_STR(SetAffectsParentSize)), &UIObject::SetAffectsParentSize, Span<const ClassAttribute> { {ClassAttribute("property", "AffectsParentSize") } }),
    Method(NAME(HYP_STR(AffectsParentSize)), &UIObject::AffectsParentSize, Span<const ClassAttribute> { {ClassAttribute("property", "AffectsParentSize") } }),
    Method(NAME(HYP_STR(GetBorderRadius)), &UIObject::GetBorderRadius, Span<const ClassAttribute> { {ClassAttribute("property", "BorderRadius") } }),
    Method(NAME(HYP_STR(SetBorderRadius)), &UIObject::SetBorderRadius, Span<const ClassAttribute> { {ClassAttribute("property", "BorderRadius") } }),
    Method(NAME(HYP_STR(GetBorderFlags)), &UIObject::GetBorderFlags, Span<const ClassAttribute> { {ClassAttribute("property", "BorderFlags") } }),
    Method(NAME(HYP_STR(SetBorderFlags)), &UIObject::SetBorderFlags, Span<const ClassAttribute> { {ClassAttribute("property", "BorderFlags") } }),
    Method(NAME(HYP_STR(GetAspectRatio)), &UIObject::GetAspectRatio, Span<const ClassAttribute> { {ClassAttribute("property", "AspectRatio") } }),
    Method(NAME(HYP_STR(SetAspectRatio)), &UIObject::SetAspectRatio, Span<const ClassAttribute> { {ClassAttribute("property", "AspectRatio") } }),
    Method(NAME(HYP_STR(GetPadding)), &UIObject::GetPadding, Span<const ClassAttribute> { {ClassAttribute("property", "Padding") } }),
    Method(NAME(HYP_STR(SetPadding)), &UIObject::SetPadding, Span<const ClassAttribute> { {ClassAttribute("property", "Padding") } }),
    Method(NAME(HYP_STR(GetBackgroundColor)), &UIObject::GetBackgroundColor, Span<const ClassAttribute> { {ClassAttribute("property", "BackgroundColor") } }),
    Method(NAME(HYP_STR(SetBackgroundColor)), &UIObject::SetBackgroundColor, Span<const ClassAttribute> { {ClassAttribute("property", "BackgroundColor") } }),
    Method(NAME(HYP_STR(GetTextColor)), &UIObject::GetTextColor, Span<const ClassAttribute> { {ClassAttribute("property", "TextColor") } }),
    Method(NAME(HYP_STR(SetTextColor)), &UIObject::SetTextColor, Span<const ClassAttribute> { {ClassAttribute("property", "TextColor") } }),
    Method(NAME(HYP_STR(GetText)), &UIObject::GetText, Span<const ClassAttribute> { {ClassAttribute("property", "Text") } }),
    Method(NAME(HYP_STR(SetText)), &UIObject::SetText, Span<const ClassAttribute> { {ClassAttribute("property", "Text") } }),
    Method(NAME(HYP_STR(GetTextSize)), &UIObject::GetTextSize, Span<const ClassAttribute> { {ClassAttribute("property", "TextSize") } }),
    Method(NAME(HYP_STR(SetTextSize)), &UIObject::SetTextSize, Span<const ClassAttribute> { {ClassAttribute("property", "TextSize") } }),
    Method(NAME(HYP_STR(IsVisible)), &UIObject::IsVisible, Span<const ClassAttribute> { {ClassAttribute("property", "IsVisible") } }),
    Method(NAME(HYP_STR(SetIsVisible)), &UIObject::SetIsVisible, Span<const ClassAttribute> { {ClassAttribute("property", "IsVisible") } }),
    Method(NAME(HYP_STR(IsEnabled)), &UIObject::IsEnabled, Span<const ClassAttribute> { {ClassAttribute("property", "IsEnabled") } }),
    Method(NAME(HYP_STR(SetIsEnabled)), &UIObject::SetIsEnabled, Span<const ClassAttribute> { {ClassAttribute("property", "IsEnabled") } }),
    Method(NAME(HYP_STR(GetParentUIObject)), &UIObject::GetParentUIObject),
    Method(NAME(HYP_STR(AddChildUIObject)), &UIObject::AddChildUIObject),
    Method(NAME(HYP_STR(RemoveChildUIObject)), &UIObject::RemoveChildUIObject),
    Method(NAME(HYP_STR(ClearDeep)), &UIObject::ClearDeep),
    Method(NAME(HYP_STR(RemoveFromParent)), &UIObject::RemoveFromParent),
    Method(NAME(HYP_STR(DetachFromParent)), &UIObject::DetachFromParent),
    Method(NAME(HYP_STR(HasChildUIObjects)), &UIObject::HasChildUIObjects),
    Method(NAME(HYP_STR(GetChildUIObject)), &UIObject::GetChildUIObject),
    Method(NAME(HYP_STR(GetNode)), &UIObject::GetNode),
    Method(NAME(HYP_STR(GetWorld)), &UIObject::GetWorld),
    Method(NAME(HYP_STR(GetAABB)), &UIObject::GetAABB),
    Method(NAME(HYP_STR(GetAABBClamped)), &UIObject::GetAABBClamped),
    Method(NAME(HYP_STR(GetDataSource)), &UIObject::GetDataSource, Span<const ClassAttribute> { {ClassAttribute("property", "DataSource") } }),
    Method(NAME(HYP_STR(SetDataSource)), &UIObject::SetDataSource, Span<const ClassAttribute> { {ClassAttribute("property", "DataSource") } }),
    Field(NAME(HYP_STR(OnInit)), &UIObject::OnInit, offsetof(UIObject, OnInit), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnAttached)), &UIObject::OnAttached, offsetof(UIObject, OnAttached), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnRemoved)), &UIObject::OnRemoved, offsetof(UIObject, OnRemoved), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnChildAttached)), &UIObject::OnChildAttached, offsetof(UIObject, OnChildAttached), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnChildRemoved)), &UIObject::OnChildRemoved, offsetof(UIObject, OnChildRemoved), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnMouseDown)), &UIObject::OnMouseDown, offsetof(UIObject, OnMouseDown), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnMouseUp)), &UIObject::OnMouseUp, offsetof(UIObject, OnMouseUp), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnMouseDrag)), &UIObject::OnMouseDrag, offsetof(UIObject, OnMouseDrag), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnMouseHover)), &UIObject::OnMouseHover, offsetof(UIObject, OnMouseHover), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnMouseLeave)), &UIObject::OnMouseLeave, offsetof(UIObject, OnMouseLeave), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnMouseMove)), &UIObject::OnMouseMove, offsetof(UIObject, OnMouseMove), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnGainFocus)), &UIObject::OnGainFocus, offsetof(UIObject, OnGainFocus), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnLoseFocus)), &UIObject::OnLoseFocus, offsetof(UIObject, OnLoseFocus), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnScroll)), &UIObject::OnScroll, offsetof(UIObject, OnScroll), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnClick)), &UIObject::OnClick, offsetof(UIObject, OnClick), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnRightClick)), &UIObject::OnRightClick, offsetof(UIObject, OnRightClick), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnKeyDown)), &UIObject::OnKeyDown, offsetof(UIObject, OnKeyDown), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnKeyUp)), &UIObject::OnKeyUp, offsetof(UIObject, OnKeyUp), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnTextChange)), &UIObject::OnTextChange, offsetof(UIObject, OnTextChange), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnSizeChange)), &UIObject::OnSizeChange, offsetof(UIObject, OnSizeChange), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnComputedVisibilityChange)), &UIObject::OnComputedVisibilityChange, offsetof(UIObject, OnComputedVisibilityChange), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnEnabled)), &UIObject::OnEnabled, offsetof(UIObject, OnEnabled), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnDisabled)), &UIObject::OnDisabled, offsetof(UIObject, OnDisabled), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnValueChange)), &UIObject::OnValueChange, offsetof(UIObject, OnValueChange), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Method(NAME(HYP_STR(Init)), &UIObject::Init)
HYP_END_CLASS

#pragma endregion UIObject Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectSize Reflection Data

HYP_BEGIN_STRUCT(UIObjectSize, 218, 0, {})
    Field(NAME(HYP_STR(Flags)), &UIObjectSize::flags, offsetof(UIObjectSize, flags)),
    Field(NAME(HYP_STR(Value)), &UIObjectSize::value, offsetof(UIObjectSize, value))
HYP_END_STRUCT

#pragma endregion UIObjectSize Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectAspectRatio Reflection Data

HYP_BEGIN_STRUCT(UIObjectAspectRatio, 219, 0, {})
    Field(NAME(HYP_STR(X)), &UIObjectAspectRatio::x, offsetof(UIObjectAspectRatio, x)),
    Field(NAME(HYP_STR(Y)), &UIObjectAspectRatio::y, offsetof(UIObjectAspectRatio, y))
HYP_END_STRUCT

#pragma endregion UIObjectAspectRatio Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectUpdateSizeFlags Reflection Data

HYP_BEGIN_ENUM(UIObjectUpdateSizeFlags, 220, 0, {})
    StaticField(NAME(HYP_STR(NONE)), UIObjectUpdateSizeFlags::NONE),
    StaticField(NAME(HYP_STR(MAX_SIZE)), UIObjectUpdateSizeFlags::MAX_SIZE),
    StaticField(NAME(HYP_STR(INNER_SIZE)), UIObjectUpdateSizeFlags::INNER_SIZE),
    StaticField(NAME(HYP_STR(OUTER_SIZE)), UIObjectUpdateSizeFlags::OUTER_SIZE),
    StaticField(NAME(HYP_STR(DEFAULT)), UIObjectUpdateSizeFlags::DEFAULT)
HYP_END_ENUM

#pragma endregion UIObjectUpdateSizeFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectAlignment Reflection Data

HYP_BEGIN_ENUM(UIObjectAlignment, 221, 0, {})
    StaticField(NAME(HYP_STR(TOP_LEFT)), UIObjectAlignment::TOP_LEFT),
    StaticField(NAME(HYP_STR(TOP_RIGHT)), UIObjectAlignment::TOP_RIGHT),
    StaticField(NAME(HYP_STR(CENTER)), UIObjectAlignment::CENTER),
    StaticField(NAME(HYP_STR(BOTTOM_LEFT)), UIObjectAlignment::BOTTOM_LEFT),
    StaticField(NAME(HYP_STR(BOTTOM_RIGHT)), UIObjectAlignment::BOTTOM_RIGHT)
HYP_END_ENUM

#pragma endregion UIObjectAlignment Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIEventHandlerResult Reflection Data

HYP_BEGIN_STRUCT(UIEventHandlerResult, 222, 0, {}, ClassAttribute("size", 24))
HYP_END_STRUCT

#pragma endregion UIEventHandlerResult Reflection Data

static_assert(sizeof(UIEventHandlerResult) == 24, "Expected sizeof(UIEventHandlerResult) to be 24 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region UIObjectBorderFlags Reflection Data

HYP_BEGIN_ENUM(UIObjectBorderFlags, 223, 0, {})
    StaticField(NAME(HYP_STR(NONE)), UIObjectBorderFlags::NONE),
    StaticField(NAME(HYP_STR(TOP)), UIObjectBorderFlags::TOP),
    StaticField(NAME(HYP_STR(LEFT)), UIObjectBorderFlags::LEFT),
    StaticField(NAME(HYP_STR(BOTTOM)), UIObjectBorderFlags::BOTTOM),
    StaticField(NAME(HYP_STR(RIGHT)), UIObjectBorderFlags::RIGHT),
    StaticField(NAME(HYP_STR(ALL)), UIObjectBorderFlags::ALL)
HYP_END_ENUM

#pragma endregion UIObjectBorderFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectUpdateType Reflection Data

HYP_BEGIN_ENUM(UIObjectUpdateType, 224, 0, {})
    StaticField(NAME(HYP_STR(NONE)), UIObjectUpdateType::NONE),
    StaticField(NAME(HYP_STR(UPDATE_SIZE)), UIObjectUpdateType::UPDATE_SIZE),
    StaticField(NAME(HYP_STR(UPDATE_POSITION)), UIObjectUpdateType::UPDATE_POSITION),
    StaticField(NAME(HYP_STR(UPDATE_MATERIAL)), UIObjectUpdateType::UPDATE_MATERIAL),
    StaticField(NAME(HYP_STR(UPDATE_MESH_DATA)), UIObjectUpdateType::UPDATE_MESH_DATA),
    StaticField(NAME(HYP_STR(UPDATE_COMPUTED_VISIBILITY)), UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY),
    StaticField(NAME(HYP_STR(UPDATE_CLAMPED_SIZE)), UIObjectUpdateType::UPDATE_CLAMPED_SIZE),
    StaticField(NAME(HYP_STR(UPDATE_CUSTOM)), UIObjectUpdateType::UPDATE_CUSTOM),
    StaticField(NAME(HYP_STR(UPDATE_ALL)), UIObjectUpdateType::UPDATE_ALL),
    StaticField(NAME(HYP_STR(UPDATE_CHILDREN_SIZE)), UIObjectUpdateType::UPDATE_CHILDREN_SIZE),
    StaticField(NAME(HYP_STR(UPDATE_CHILDREN_POSITION)), UIObjectUpdateType::UPDATE_CHILDREN_POSITION),
    StaticField(NAME(HYP_STR(UPDATE_CHILDREN_MATERIAL)), UIObjectUpdateType::UPDATE_CHILDREN_MATERIAL),
    StaticField(NAME(HYP_STR(UPDATE_CHILDREN_MESH_DATA)), UIObjectUpdateType::UPDATE_CHILDREN_MESH_DATA),
    StaticField(NAME(HYP_STR(UPDATE_CHILDREN_COMPUTED_VISIBILITY)), UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY),
    StaticField(NAME(HYP_STR(UPDATE_CHILDREN_CLAMPED_SIZE)), UIObjectUpdateType::UPDATE_CHILDREN_CLAMPED_SIZE),
    StaticField(NAME(HYP_STR(UPDATE_CHILDREN_CUSTOM)), UIObjectUpdateType::UPDATE_CHILDREN_CUSTOM),
    StaticField(NAME(HYP_STR(UPDATE_CHILDREN_ALL)), UIObjectUpdateType::UPDATE_CHILDREN_ALL)
HYP_END_ENUM

#pragma endregion UIObjectUpdateType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectFocusState Reflection Data

HYP_BEGIN_ENUM(UIObjectFocusState, 225, 0, {})
    StaticField(NAME(HYP_STR(NONE)), UIObjectFocusState::NONE),
    StaticField(NAME(HYP_STR(HOVER)), UIObjectFocusState::HOVER),
    StaticField(NAME(HYP_STR(PRESSED)), UIObjectFocusState::PRESSED),
    StaticField(NAME(HYP_STR(TOGGLED)), UIObjectFocusState::TOGGLED),
    StaticField(NAME(HYP_STR(FOCUSED)), UIObjectFocusState::FOCUSED)
HYP_END_ENUM

#pragma endregion UIObjectFocusState Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ScrollAxis Reflection Data

HYP_BEGIN_ENUM(ScrollAxis, 226, 0, {})
    StaticField(NAME(HYP_STR(SA_NONE)), ScrollAxis::SA_NONE),
    StaticField(NAME(HYP_STR(SA_HORIZONTAL)), ScrollAxis::SA_HORIZONTAL),
    StaticField(NAME(HYP_STR(SA_VERTICAL)), ScrollAxis::SA_VERTICAL),
    StaticField(NAME(HYP_STR(SA_ALL)), ScrollAxis::SA_ALL)
HYP_END_ENUM

#pragma endregion ScrollAxis Reflection Data

} // namespace hyperion

