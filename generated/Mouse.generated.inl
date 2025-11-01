#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region MouseButtonState Reflection Data

HYP_BEGIN_ENUM(MouseButtonState, 260, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), MouseButtonState::NONE),
    HypConstant(NAME(HYP_STR(LEFT)), MouseButtonState::LEFT),
    HypConstant(NAME(HYP_STR(MIDDLE)), MouseButtonState::MIDDLE),
    HypConstant(NAME(HYP_STR(RIGHT)), MouseButtonState::RIGHT)
HYP_END_ENUM

#pragma endregion MouseButtonState Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MouseEvent Reflection Data

HYP_BEGIN_STRUCT(MouseEvent, 261, 0, {}, HypClassAttribute("size", 56))
    HypField(NAME(HYP_STR(InputManager)), &MouseEvent::inputManager, offsetof(MouseEvent, inputManager)),
    HypField(NAME(HYP_STR(Position)), &MouseEvent::position, offsetof(MouseEvent, position)),
    HypField(NAME(HYP_STR(PreviousPosition)), &MouseEvent::previousPosition, offsetof(MouseEvent, previousPosition)),
    HypField(NAME(HYP_STR(AbsolutePosition)), &MouseEvent::absolutePosition, offsetof(MouseEvent, absolutePosition)),
    HypField(NAME(HYP_STR(MouseButtons)), &MouseEvent::mouseButtons, offsetof(MouseEvent, mouseButtons)),
    HypField(NAME(HYP_STR(Wheel)), &MouseEvent::wheel, offsetof(MouseEvent, wheel)),
    HypField(NAME(HYP_STR(IsDown)), &MouseEvent::isDown, offsetof(MouseEvent, isDown), Span<const HypClassAttribute> { {HypClassAttribute("deprecated", true) } })
HYP_END_STRUCT

#pragma endregion MouseEvent Reflection Data

static_assert(sizeof(MouseEvent) == 56, "Expected sizeof(MouseEvent) to be 56 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region MouseButtonKey Reflection Data

HYP_BEGIN_ENUM(MouseButtonKey, 262, 0, {})
    HypConstant(NAME(HYP_STR(MBK_INVALID)), MouseButtonKey::MBK_INVALID),
    HypConstant(NAME(HYP_STR(MBK_LEFT)), MouseButtonKey::MBK_LEFT),
    HypConstant(NAME(HYP_STR(MBK_MIDDLE)), MouseButtonKey::MBK_MIDDLE),
    HypConstant(NAME(HYP_STR(MBK_RIGHT)), MouseButtonKey::MBK_RIGHT),
    HypConstant(NAME(HYP_STR(MBK_MAX)), MouseButtonKey::MBK_MAX)
HYP_END_ENUM

#pragma endregion MouseButtonKey Reflection Data

} // namespace hyperion

