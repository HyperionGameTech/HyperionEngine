#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region KeyCode Reflection Data

HYP_BEGIN_ENUM(KeyCode, 258, 0, {})
    HypConstant(NAME(HYP_STR(UNKNOWN)), KeyCode::UNKNOWN),
    HypConstant(NAME(HYP_STR(KEY_A)), KeyCode::KEY_A),
    HypConstant(NAME(HYP_STR(KEY_B)), KeyCode::KEY_B),
    HypConstant(NAME(HYP_STR(KEY_C)), KeyCode::KEY_C),
    HypConstant(NAME(HYP_STR(KEY_D)), KeyCode::KEY_D),
    HypConstant(NAME(HYP_STR(KEY_E)), KeyCode::KEY_E),
    HypConstant(NAME(HYP_STR(KEY_F)), KeyCode::KEY_F),
    HypConstant(NAME(HYP_STR(KEY_G)), KeyCode::KEY_G),
    HypConstant(NAME(HYP_STR(KEY_H)), KeyCode::KEY_H),
    HypConstant(NAME(HYP_STR(KEY_I)), KeyCode::KEY_I),
    HypConstant(NAME(HYP_STR(KEY_J)), KeyCode::KEY_J),
    HypConstant(NAME(HYP_STR(KEY_K)), KeyCode::KEY_K),
    HypConstant(NAME(HYP_STR(KEY_L)), KeyCode::KEY_L),
    HypConstant(NAME(HYP_STR(KEY_M)), KeyCode::KEY_M),
    HypConstant(NAME(HYP_STR(KEY_N)), KeyCode::KEY_N),
    HypConstant(NAME(HYP_STR(KEY_O)), KeyCode::KEY_O),
    HypConstant(NAME(HYP_STR(KEY_P)), KeyCode::KEY_P),
    HypConstant(NAME(HYP_STR(KEY_Q)), KeyCode::KEY_Q),
    HypConstant(NAME(HYP_STR(KEY_R)), KeyCode::KEY_R),
    HypConstant(NAME(HYP_STR(KEY_S)), KeyCode::KEY_S),
    HypConstant(NAME(HYP_STR(KEY_T)), KeyCode::KEY_T),
    HypConstant(NAME(HYP_STR(KEY_U)), KeyCode::KEY_U),
    HypConstant(NAME(HYP_STR(KEY_V)), KeyCode::KEY_V),
    HypConstant(NAME(HYP_STR(KEY_W)), KeyCode::KEY_W),
    HypConstant(NAME(HYP_STR(KEY_X)), KeyCode::KEY_X),
    HypConstant(NAME(HYP_STR(KEY_Y)), KeyCode::KEY_Y),
    HypConstant(NAME(HYP_STR(KEY_Z)), KeyCode::KEY_Z),
    HypConstant(NAME(HYP_STR(KEY_0)), KeyCode::KEY_0),
    HypConstant(NAME(HYP_STR(KEY_1)), KeyCode::KEY_1),
    HypConstant(NAME(HYP_STR(KEY_2)), KeyCode::KEY_2),
    HypConstant(NAME(HYP_STR(KEY_3)), KeyCode::KEY_3),
    HypConstant(NAME(HYP_STR(KEY_4)), KeyCode::KEY_4),
    HypConstant(NAME(HYP_STR(KEY_5)), KeyCode::KEY_5),
    HypConstant(NAME(HYP_STR(KEY_6)), KeyCode::KEY_6),
    HypConstant(NAME(HYP_STR(KEY_7)), KeyCode::KEY_7),
    HypConstant(NAME(HYP_STR(KEY_8)), KeyCode::KEY_8),
    HypConstant(NAME(HYP_STR(KEY_9)), KeyCode::KEY_9),
    HypConstant(NAME(HYP_STR(KEY_F1)), KeyCode::KEY_F1),
    HypConstant(NAME(HYP_STR(KEY_F2)), KeyCode::KEY_F2),
    HypConstant(NAME(HYP_STR(KEY_F3)), KeyCode::KEY_F3),
    HypConstant(NAME(HYP_STR(KEY_F4)), KeyCode::KEY_F4),
    HypConstant(NAME(HYP_STR(KEY_F5)), KeyCode::KEY_F5),
    HypConstant(NAME(HYP_STR(KEY_F6)), KeyCode::KEY_F6),
    HypConstant(NAME(HYP_STR(KEY_F7)), KeyCode::KEY_F7),
    HypConstant(NAME(HYP_STR(KEY_F8)), KeyCode::KEY_F8),
    HypConstant(NAME(HYP_STR(KEY_F9)), KeyCode::KEY_F9),
    HypConstant(NAME(HYP_STR(KEY_F10)), KeyCode::KEY_F10),
    HypConstant(NAME(HYP_STR(KEY_F11)), KeyCode::KEY_F11),
    HypConstant(NAME(HYP_STR(KEY_F12)), KeyCode::KEY_F12),
    HypConstant(NAME(HYP_STR(LEFT_CTRL)), KeyCode::LEFT_CTRL),
    HypConstant(NAME(HYP_STR(LEFT_SHIFT)), KeyCode::LEFT_SHIFT),
    HypConstant(NAME(HYP_STR(LEFT_ALT)), KeyCode::LEFT_ALT),
    HypConstant(NAME(HYP_STR(RIGHT_CTRL)), KeyCode::RIGHT_CTRL),
    HypConstant(NAME(HYP_STR(RIGHT_SHIFT)), KeyCode::RIGHT_SHIFT),
    HypConstant(NAME(HYP_STR(RIGHT_ALT)), KeyCode::RIGHT_ALT),
    HypConstant(NAME(HYP_STR(SPACE)), KeyCode::SPACE),
    HypConstant(NAME(HYP_STR(COMMA)), KeyCode::COMMA),
    HypConstant(NAME(HYP_STR(DASH)), KeyCode::DASH),
    HypConstant(NAME(HYP_STR(PERIOD)), KeyCode::PERIOD),
    HypConstant(NAME(HYP_STR(RETURN)), KeyCode::RETURN),
    HypConstant(NAME(HYP_STR(TAB)), KeyCode::TAB),
    HypConstant(NAME(HYP_STR(BACKSPACE)), KeyCode::BACKSPACE),
    HypConstant(NAME(HYP_STR(CAPSLOCK)), KeyCode::CAPSLOCK),
    HypConstant(NAME(HYP_STR(TILDE)), KeyCode::TILDE),
    HypConstant(NAME(HYP_STR(ARROW_RIGHT)), KeyCode::ARROW_RIGHT),
    HypConstant(NAME(HYP_STR(ARROW_LEFT)), KeyCode::ARROW_LEFT),
    HypConstant(NAME(HYP_STR(ARROW_DOWN)), KeyCode::ARROW_DOWN),
    HypConstant(NAME(HYP_STR(ARROW_UP)), KeyCode::ARROW_UP),
    HypConstant(NAME(HYP_STR(ESC)), KeyCode::ESC)
HYP_END_ENUM

#pragma endregion KeyCode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region KeyboardEvent Reflection Data

HYP_BEGIN_STRUCT(KeyboardEvent, 259, 0, {}, HypClassAttribute("size", 16))
    HypField(NAME(HYP_STR(InputManager)), &KeyboardEvent::inputManager, offsetof(KeyboardEvent, inputManager)),
    HypField(NAME(HYP_STR(KeyCode)), &KeyboardEvent::keyCode, offsetof(KeyboardEvent, keyCode))
HYP_END_STRUCT

#pragma endregion KeyboardEvent Reflection Data

static_assert(sizeof(KeyboardEvent) == 16, "Expected sizeof(KeyboardEvent) to be 16 bytes");
} // namespace hyperion

