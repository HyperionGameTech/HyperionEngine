#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region UIObject Reflection Data

HYP_BEGIN_CLASS(UIObject, 193, 22, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
    HypMethod(NAME(HYP_STR(GetEntity)), &UIObject::GetEntity),
    HypMethod(NAME(HYP_STR(GetStage)), &UIObject::GetStage),
    HypMethod(NAME(HYP_STR(SetStage)), &UIObject::SetStage),
    HypMethod(NAME(HYP_STR(GetName)), &UIObject::GetName, Span<const HypClassAttribute> { {HypClassAttribute("property", "Name") } }),
    HypMethod(NAME(HYP_STR(SetName)), &UIObject::SetName, Span<const HypClassAttribute> { {HypClassAttribute("property", "Name") } }),
    HypMethod(NAME(HYP_STR(GetPosition)), &UIObject::GetPosition, Span<const HypClassAttribute> { {HypClassAttribute("property", "Position") } }),
    HypMethod(NAME(HYP_STR(SetPosition)), &UIObject::SetPosition, Span<const HypClassAttribute> { {HypClassAttribute("property", "Position") } }),
    HypMethod(NAME(HYP_STR(GetOffsetPosition)), &UIObject::GetOffsetPosition),
    HypMethod(NAME(HYP_STR(GetAbsolutePosition)), &UIObject::GetAbsolutePosition),
    HypMethod(NAME(HYP_STR(IsPositionAbsolute)), &UIObject::IsPositionAbsolute, Span<const HypClassAttribute> { {HypClassAttribute("property", "IsPositionAbsolute"), HypClassAttribute("xmlattribute", "absolute") } }),
    HypMethod(NAME(HYP_STR(SetIsPositionAbsolute)), &UIObject::SetIsPositionAbsolute, Span<const HypClassAttribute> { {HypClassAttribute("property", "IsPositionAbsolute"), HypClassAttribute("xmlattribute", "absolute") } }),
    HypMethod(NAME(HYP_STR(GetSize)), &UIObject::GetSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "Size") } }),
    HypMethod(NAME(HYP_STR(SetSize)), &UIObject::SetSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "Size") } }),
    HypMethod(NAME(HYP_STR(GetInnerSize)), &UIObject::GetInnerSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "InnerSize") } }),
    HypMethod(NAME(HYP_STR(SetInnerSize)), &UIObject::SetInnerSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "InnerSize") } }),
    HypMethod(NAME(HYP_STR(GetMaxSize)), &UIObject::GetMaxSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "MaxSize") } }),
    HypMethod(NAME(HYP_STR(SetMaxSize)), &UIObject::SetMaxSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "MaxSize") } }),
    HypMethod(NAME(HYP_STR(GetActualSize)), &UIObject::GetActualSize),
    HypMethod(NAME(HYP_STR(GetActualSizeClamped)), &UIObject::GetActualSizeClamped),
    HypMethod(NAME(HYP_STR(GetActualInnerSize)), &UIObject::GetActualInnerSize),
    HypMethod(NAME(HYP_STR(GetScrollOffset)), &UIObject::GetScrollOffset),
    HypMethod(NAME(HYP_STR(SetScrollOffset)), &UIObject::SetScrollOffset),
    HypMethod(NAME(HYP_STR(ScrollToChild)), &UIObject::ScrollToChild),
    HypMethod(NAME(HYP_STR(GetVerticalScrollbarSize)), &UIObject::GetVerticalScrollbarSize),
    HypMethod(NAME(HYP_STR(GetHorizontalScrollbarSize)), &UIObject::GetHorizontalScrollbarSize),
    HypMethod(NAME(HYP_STR(CanScrollOnAxis)), &UIObject::CanScrollOnAxis),
    HypMethod(NAME(HYP_STR(GetComputedDepth)), &UIObject::GetComputedDepth),
    HypMethod(NAME(HYP_STR(GetDepth)), &UIObject::GetDepth, Span<const HypClassAttribute> { {HypClassAttribute("property", "Depth") } }),
    HypMethod(NAME(HYP_STR(SetDepth)), &UIObject::SetDepth, Span<const HypClassAttribute> { {HypClassAttribute("property", "Depth") } }),
    HypMethod(NAME(HYP_STR(AcceptsFocus)), &UIObject::AcceptsFocus, Span<const HypClassAttribute> { {HypClassAttribute("property", "AcceptsFocus") } }),
    HypMethod(NAME(HYP_STR(SetAcceptsFocus)), &UIObject::SetAcceptsFocus, Span<const HypClassAttribute> { {HypClassAttribute("property", "AcceptsFocus") } }),
    HypMethod(NAME(HYP_STR(NeedsUpdate)), &UIObject::NeedsUpdate),
    HypMethod(NAME(HYP_STR(Focus)), &UIObject::Focus),
    HypMethod(NAME(HYP_STR(Blur)), &UIObject::Blur),
    HypMethod(NAME(HYP_STR(SetAffectsParentSize)), &UIObject::SetAffectsParentSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "AffectsParentSize") } }),
    HypMethod(NAME(HYP_STR(AffectsParentSize)), &UIObject::AffectsParentSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "AffectsParentSize") } }),
    HypMethod(NAME(HYP_STR(GetBorderRadius)), &UIObject::GetBorderRadius, Span<const HypClassAttribute> { {HypClassAttribute("property", "BorderRadius") } }),
    HypMethod(NAME(HYP_STR(SetBorderRadius)), &UIObject::SetBorderRadius, Span<const HypClassAttribute> { {HypClassAttribute("property", "BorderRadius") } }),
    HypMethod(NAME(HYP_STR(GetBorderFlags)), &UIObject::GetBorderFlags, Span<const HypClassAttribute> { {HypClassAttribute("property", "BorderFlags") } }),
    HypMethod(NAME(HYP_STR(SetBorderFlags)), &UIObject::SetBorderFlags, Span<const HypClassAttribute> { {HypClassAttribute("property", "BorderFlags") } }),
    HypMethod(NAME(HYP_STR(GetAspectRatio)), &UIObject::GetAspectRatio, Span<const HypClassAttribute> { {HypClassAttribute("property", "AspectRatio") } }),
    HypMethod(NAME(HYP_STR(SetAspectRatio)), &UIObject::SetAspectRatio, Span<const HypClassAttribute> { {HypClassAttribute("property", "AspectRatio") } }),
    HypMethod(NAME(HYP_STR(GetPadding)), &UIObject::GetPadding, Span<const HypClassAttribute> { {HypClassAttribute("property", "Padding") } }),
    HypMethod(NAME(HYP_STR(SetPadding)), &UIObject::SetPadding, Span<const HypClassAttribute> { {HypClassAttribute("property", "Padding") } }),
    HypMethod(NAME(HYP_STR(GetBackgroundColor)), &UIObject::GetBackgroundColor, Span<const HypClassAttribute> { {HypClassAttribute("property", "BackgroundColor") } }),
    HypMethod(NAME(HYP_STR(SetBackgroundColor)), &UIObject::SetBackgroundColor, Span<const HypClassAttribute> { {HypClassAttribute("property", "BackgroundColor") } }),
    HypMethod(NAME(HYP_STR(GetTextColor)), &UIObject::GetTextColor, Span<const HypClassAttribute> { {HypClassAttribute("property", "TextColor") } }),
    HypMethod(NAME(HYP_STR(SetTextColor)), &UIObject::SetTextColor, Span<const HypClassAttribute> { {HypClassAttribute("property", "TextColor") } }),
    HypMethod(NAME(HYP_STR(GetText)), &UIObject::GetText, Span<const HypClassAttribute> { {HypClassAttribute("property", "Text") } }),
    HypMethod(NAME(HYP_STR(SetText)), &UIObject::SetText, Span<const HypClassAttribute> { {HypClassAttribute("property", "Text") } }),
    HypMethod(NAME(HYP_STR(GetTextSize)), &UIObject::GetTextSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "TextSize") } }),
    HypMethod(NAME(HYP_STR(SetTextSize)), &UIObject::SetTextSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "TextSize") } }),
    HypMethod(NAME(HYP_STR(IsVisible)), &UIObject::IsVisible, Span<const HypClassAttribute> { {HypClassAttribute("property", "IsVisible") } }),
    HypMethod(NAME(HYP_STR(SetIsVisible)), &UIObject::SetIsVisible, Span<const HypClassAttribute> { {HypClassAttribute("property", "IsVisible") } }),
    HypMethod(NAME(HYP_STR(IsEnabled)), &UIObject::IsEnabled, Span<const HypClassAttribute> { {HypClassAttribute("property", "IsEnabled") } }),
    HypMethod(NAME(HYP_STR(SetIsEnabled)), &UIObject::SetIsEnabled, Span<const HypClassAttribute> { {HypClassAttribute("property", "IsEnabled") } }),
    HypMethod(NAME(HYP_STR(GetParentUIObject)), &UIObject::GetParentUIObject),
    HypMethod(NAME(HYP_STR(AddChildUIObject)), &UIObject::AddChildUIObject),
    HypMethod(NAME(HYP_STR(RemoveChildUIObject)), &UIObject::RemoveChildUIObject),
    HypMethod(NAME(HYP_STR(ClearDeep)), &UIObject::ClearDeep),
    HypMethod(NAME(HYP_STR(RemoveFromParent)), &UIObject::RemoveFromParent),
    HypMethod(NAME(HYP_STR(DetachFromParent)), &UIObject::DetachFromParent),
    HypMethod(NAME(HYP_STR(HasChildUIObjects)), &UIObject::HasChildUIObjects),
    HypMethod(NAME(HYP_STR(GetChildUIObject)), &UIObject::GetChildUIObject),
    HypMethod(NAME(HYP_STR(GetNode)), &UIObject::GetNode),
    HypMethod(NAME(HYP_STR(GetWorld)), &UIObject::GetWorld),
    HypMethod(NAME(HYP_STR(GetAABB)), &UIObject::GetAABB),
    HypMethod(NAME(HYP_STR(GetAABBClamped)), &UIObject::GetAABBClamped),
    HypMethod(NAME(HYP_STR(GetDataSource)), &UIObject::GetDataSource, Span<const HypClassAttribute> { {HypClassAttribute("property", "DataSource") } }),
    HypMethod(NAME(HYP_STR(SetDataSource)), &UIObject::SetDataSource, Span<const HypClassAttribute> { {HypClassAttribute("property", "DataSource") } }),
    HypField(NAME(HYP_STR(OnInit)), &UIObject::OnInit, offsetof(UIObject, OnInit), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnAttached)), &UIObject::OnAttached, offsetof(UIObject, OnAttached), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnRemoved)), &UIObject::OnRemoved, offsetof(UIObject, OnRemoved), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnChildAttached)), &UIObject::OnChildAttached, offsetof(UIObject, OnChildAttached), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnChildRemoved)), &UIObject::OnChildRemoved, offsetof(UIObject, OnChildRemoved), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnMouseDown)), &UIObject::OnMouseDown, offsetof(UIObject, OnMouseDown), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnMouseUp)), &UIObject::OnMouseUp, offsetof(UIObject, OnMouseUp), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnMouseDrag)), &UIObject::OnMouseDrag, offsetof(UIObject, OnMouseDrag), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnMouseHover)), &UIObject::OnMouseHover, offsetof(UIObject, OnMouseHover), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnMouseLeave)), &UIObject::OnMouseLeave, offsetof(UIObject, OnMouseLeave), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnMouseMove)), &UIObject::OnMouseMove, offsetof(UIObject, OnMouseMove), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnGainFocus)), &UIObject::OnGainFocus, offsetof(UIObject, OnGainFocus), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnLoseFocus)), &UIObject::OnLoseFocus, offsetof(UIObject, OnLoseFocus), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnScroll)), &UIObject::OnScroll, offsetof(UIObject, OnScroll), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnClick)), &UIObject::OnClick, offsetof(UIObject, OnClick), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnRightClick)), &UIObject::OnRightClick, offsetof(UIObject, OnRightClick), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnKeyDown)), &UIObject::OnKeyDown, offsetof(UIObject, OnKeyDown), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnKeyUp)), &UIObject::OnKeyUp, offsetof(UIObject, OnKeyUp), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnTextChange)), &UIObject::OnTextChange, offsetof(UIObject, OnTextChange), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnSizeChange)), &UIObject::OnSizeChange, offsetof(UIObject, OnSizeChange), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnComputedVisibilityChange)), &UIObject::OnComputedVisibilityChange, offsetof(UIObject, OnComputedVisibilityChange), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnEnabled)), &UIObject::OnEnabled, offsetof(UIObject, OnEnabled), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnDisabled)), &UIObject::OnDisabled, offsetof(UIObject, OnDisabled), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnValueChange)), &UIObject::OnValueChange, offsetof(UIObject, OnValueChange), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypMethod(NAME(HYP_STR(Init)), &UIObject::Init)
HYP_END_CLASS

#pragma endregion UIObject Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectSize Reflection Data

HYP_BEGIN_STRUCT(UIObjectSize, 408, 0, {})
    HypField(NAME(HYP_STR(Flags)), &UIObjectSize::flags, offsetof(UIObjectSize, flags)),
    HypField(NAME(HYP_STR(Value)), &UIObjectSize::value, offsetof(UIObjectSize, value))
HYP_END_STRUCT

#pragma endregion UIObjectSize Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectAspectRatio Reflection Data

HYP_BEGIN_STRUCT(UIObjectAspectRatio, 409, 0, {})
    HypField(NAME(HYP_STR(X)), &UIObjectAspectRatio::x, offsetof(UIObjectAspectRatio, x)),
    HypField(NAME(HYP_STR(Y)), &UIObjectAspectRatio::y, offsetof(UIObjectAspectRatio, y))
HYP_END_STRUCT

#pragma endregion UIObjectAspectRatio Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectUpdateSizeFlags Reflection Data

HYP_BEGIN_ENUM(UIObjectUpdateSizeFlags, 410, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), UIObjectUpdateSizeFlags::NONE),
    HypConstant(NAME(HYP_STR(MAX_SIZE)), UIObjectUpdateSizeFlags::MAX_SIZE),
    HypConstant(NAME(HYP_STR(INNER_SIZE)), UIObjectUpdateSizeFlags::INNER_SIZE),
    HypConstant(NAME(HYP_STR(OUTER_SIZE)), UIObjectUpdateSizeFlags::OUTER_SIZE),
    HypConstant(NAME(HYP_STR(DEFAULT)), UIObjectUpdateSizeFlags::DEFAULT)
HYP_END_ENUM

#pragma endregion UIObjectUpdateSizeFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectAlignment Reflection Data

HYP_BEGIN_ENUM(UIObjectAlignment, 411, 0, {})
    HypConstant(NAME(HYP_STR(TOP_LEFT)), UIObjectAlignment::TOP_LEFT),
    HypConstant(NAME(HYP_STR(TOP_RIGHT)), UIObjectAlignment::TOP_RIGHT),
    HypConstant(NAME(HYP_STR(CENTER)), UIObjectAlignment::CENTER),
    HypConstant(NAME(HYP_STR(BOTTOM_LEFT)), UIObjectAlignment::BOTTOM_LEFT),
    HypConstant(NAME(HYP_STR(BOTTOM_RIGHT)), UIObjectAlignment::BOTTOM_RIGHT)
HYP_END_ENUM

#pragma endregion UIObjectAlignment Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIEventHandlerResult Reflection Data

HYP_BEGIN_STRUCT(UIEventHandlerResult, 412, 0, {}, HypClassAttribute("size", 24))
HYP_END_STRUCT

#pragma endregion UIEventHandlerResult Reflection Data

static_assert(sizeof(UIEventHandlerResult) == 24, "Expected sizeof(UIEventHandlerResult) to be 24 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region UIObjectBorderFlags Reflection Data

HYP_BEGIN_ENUM(UIObjectBorderFlags, 413, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), UIObjectBorderFlags::NONE),
    HypConstant(NAME(HYP_STR(TOP)), UIObjectBorderFlags::TOP),
    HypConstant(NAME(HYP_STR(LEFT)), UIObjectBorderFlags::LEFT),
    HypConstant(NAME(HYP_STR(BOTTOM)), UIObjectBorderFlags::BOTTOM),
    HypConstant(NAME(HYP_STR(RIGHT)), UIObjectBorderFlags::RIGHT),
    HypConstant(NAME(HYP_STR(ALL)), UIObjectBorderFlags::ALL)
HYP_END_ENUM

#pragma endregion UIObjectBorderFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectUpdateType Reflection Data

HYP_BEGIN_ENUM(UIObjectUpdateType, 414, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), UIObjectUpdateType::NONE),
    HypConstant(NAME(HYP_STR(UPDATE_SIZE)), UIObjectUpdateType::UPDATE_SIZE),
    HypConstant(NAME(HYP_STR(UPDATE_POSITION)), UIObjectUpdateType::UPDATE_POSITION),
    HypConstant(NAME(HYP_STR(UPDATE_MATERIAL)), UIObjectUpdateType::UPDATE_MATERIAL),
    HypConstant(NAME(HYP_STR(UPDATE_MESH_DATA)), UIObjectUpdateType::UPDATE_MESH_DATA),
    HypConstant(NAME(HYP_STR(UPDATE_COMPUTED_VISIBILITY)), UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY),
    HypConstant(NAME(HYP_STR(UPDATE_CLAMPED_SIZE)), UIObjectUpdateType::UPDATE_CLAMPED_SIZE),
    HypConstant(NAME(HYP_STR(UPDATE_CUSTOM)), UIObjectUpdateType::UPDATE_CUSTOM),
    HypConstant(NAME(HYP_STR(UPDATE_ALL)), UIObjectUpdateType::UPDATE_ALL),
    HypConstant(NAME(HYP_STR(UPDATE_CHILDREN_SIZE)), UIObjectUpdateType::UPDATE_CHILDREN_SIZE),
    HypConstant(NAME(HYP_STR(UPDATE_CHILDREN_POSITION)), UIObjectUpdateType::UPDATE_CHILDREN_POSITION),
    HypConstant(NAME(HYP_STR(UPDATE_CHILDREN_MATERIAL)), UIObjectUpdateType::UPDATE_CHILDREN_MATERIAL),
    HypConstant(NAME(HYP_STR(UPDATE_CHILDREN_MESH_DATA)), UIObjectUpdateType::UPDATE_CHILDREN_MESH_DATA),
    HypConstant(NAME(HYP_STR(UPDATE_CHILDREN_COMPUTED_VISIBILITY)), UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY),
    HypConstant(NAME(HYP_STR(UPDATE_CHILDREN_CLAMPED_SIZE)), UIObjectUpdateType::UPDATE_CHILDREN_CLAMPED_SIZE),
    HypConstant(NAME(HYP_STR(UPDATE_CHILDREN_CUSTOM)), UIObjectUpdateType::UPDATE_CHILDREN_CUSTOM),
    HypConstant(NAME(HYP_STR(UPDATE_CHILDREN_ALL)), UIObjectUpdateType::UPDATE_CHILDREN_ALL)
HYP_END_ENUM

#pragma endregion UIObjectUpdateType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIObjectFocusState Reflection Data

HYP_BEGIN_ENUM(UIObjectFocusState, 415, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), UIObjectFocusState::NONE),
    HypConstant(NAME(HYP_STR(HOVER)), UIObjectFocusState::HOVER),
    HypConstant(NAME(HYP_STR(PRESSED)), UIObjectFocusState::PRESSED),
    HypConstant(NAME(HYP_STR(TOGGLED)), UIObjectFocusState::TOGGLED),
    HypConstant(NAME(HYP_STR(FOCUSED)), UIObjectFocusState::FOCUSED)
HYP_END_ENUM

#pragma endregion UIObjectFocusState Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ScrollAxis Reflection Data

HYP_BEGIN_ENUM(ScrollAxis, 416, 0, {})
    HypConstant(NAME(HYP_STR(SA_NONE)), ScrollAxis::SA_NONE),
    HypConstant(NAME(HYP_STR(SA_HORIZONTAL)), ScrollAxis::SA_HORIZONTAL),
    HypConstant(NAME(HYP_STR(SA_VERTICAL)), ScrollAxis::SA_VERTICAL),
    HypConstant(NAME(HYP_STR(SA_ALL)), ScrollAxis::SA_ALL)
HYP_END_ENUM

#pragma endregion ScrollAxis Reflection Data

} // namespace hyperion

