#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region MouseButtonState Reflection Data

HYP_BEGIN_ENUM(MouseButtonState, 271, 0, {})
    StaticField(NAME(HYP_STR(NONE)), MouseButtonState::NONE),
    StaticField(NAME(HYP_STR(LEFT)), MouseButtonState::LEFT),
    StaticField(NAME(HYP_STR(MIDDLE)), MouseButtonState::MIDDLE),
    StaticField(NAME(HYP_STR(RIGHT)), MouseButtonState::RIGHT)
HYP_END_ENUM

#pragma endregion MouseButtonState Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MouseEvent Reflection Data

HYP_BEGIN_STRUCT(MouseEvent, 272, 0, {}, ClassAttribute("size", 56))
    Field(NAME(HYP_STR(InputManager)), &MouseEvent::inputManager, offsetof(MouseEvent, inputManager)),
    Field(NAME(HYP_STR(Position)), &MouseEvent::position, offsetof(MouseEvent, position)),
    Field(NAME(HYP_STR(PreviousPosition)), &MouseEvent::previousPosition, offsetof(MouseEvent, previousPosition)),
    Field(NAME(HYP_STR(AbsolutePosition)), &MouseEvent::absolutePosition, offsetof(MouseEvent, absolutePosition)),
    Field(NAME(HYP_STR(MouseButtons)), &MouseEvent::mouseButtons, offsetof(MouseEvent, mouseButtons)),
    Field(NAME(HYP_STR(Wheel)), &MouseEvent::wheel, offsetof(MouseEvent, wheel)),
    Field(NAME(HYP_STR(IsDown)), &MouseEvent::isDown, offsetof(MouseEvent, isDown), Span<const ClassAttribute> { {ClassAttribute("deprecated", true) } })
HYP_END_STRUCT

#pragma endregion MouseEvent Reflection Data

static_assert(sizeof(MouseEvent) == 56, "Expected sizeof(MouseEvent) to be 56 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region MouseButtonKey Reflection Data

HYP_BEGIN_ENUM(MouseButtonKey, 273, 0, {})
    StaticField(NAME(HYP_STR(MBK_INVALID)), MouseButtonKey::MBK_INVALID),
    StaticField(NAME(HYP_STR(MBK_LEFT)), MouseButtonKey::MBK_LEFT),
    StaticField(NAME(HYP_STR(MBK_MIDDLE)), MouseButtonKey::MBK_MIDDLE),
    StaticField(NAME(HYP_STR(MBK_RIGHT)), MouseButtonKey::MBK_RIGHT),
    StaticField(NAME(HYP_STR(MBK_MAX)), MouseButtonKey::MBK_MAX)
HYP_END_ENUM

#pragma endregion MouseButtonKey Reflection Data

} // namespace hyperion

