#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region KeyCode Reflection Data

HYP_BEGIN_ENUM(KeyCode, 275, 0, {})
    StaticField(NAME(HYP_STR(KEY_UNKNOWN)), KeyCode::KEY_UNKNOWN),
    StaticField(NAME(HYP_STR(KEY_A)), KeyCode::KEY_A),
    StaticField(NAME(HYP_STR(KEY_B)), KeyCode::KEY_B),
    StaticField(NAME(HYP_STR(KEY_C)), KeyCode::KEY_C),
    StaticField(NAME(HYP_STR(KEY_D)), KeyCode::KEY_D),
    StaticField(NAME(HYP_STR(KEY_E)), KeyCode::KEY_E),
    StaticField(NAME(HYP_STR(KEY_F)), KeyCode::KEY_F),
    StaticField(NAME(HYP_STR(KEY_G)), KeyCode::KEY_G),
    StaticField(NAME(HYP_STR(KEY_H)), KeyCode::KEY_H),
    StaticField(NAME(HYP_STR(KEY_I)), KeyCode::KEY_I),
    StaticField(NAME(HYP_STR(KEY_J)), KeyCode::KEY_J),
    StaticField(NAME(HYP_STR(KEY_K)), KeyCode::KEY_K),
    StaticField(NAME(HYP_STR(KEY_L)), KeyCode::KEY_L),
    StaticField(NAME(HYP_STR(KEY_M)), KeyCode::KEY_M),
    StaticField(NAME(HYP_STR(KEY_N)), KeyCode::KEY_N),
    StaticField(NAME(HYP_STR(KEY_O)), KeyCode::KEY_O),
    StaticField(NAME(HYP_STR(KEY_P)), KeyCode::KEY_P),
    StaticField(NAME(HYP_STR(KEY_Q)), KeyCode::KEY_Q),
    StaticField(NAME(HYP_STR(KEY_R)), KeyCode::KEY_R),
    StaticField(NAME(HYP_STR(KEY_S)), KeyCode::KEY_S),
    StaticField(NAME(HYP_STR(KEY_T)), KeyCode::KEY_T),
    StaticField(NAME(HYP_STR(KEY_U)), KeyCode::KEY_U),
    StaticField(NAME(HYP_STR(KEY_V)), KeyCode::KEY_V),
    StaticField(NAME(HYP_STR(KEY_W)), KeyCode::KEY_W),
    StaticField(NAME(HYP_STR(KEY_X)), KeyCode::KEY_X),
    StaticField(NAME(HYP_STR(KEY_Y)), KeyCode::KEY_Y),
    StaticField(NAME(HYP_STR(KEY_Z)), KeyCode::KEY_Z),
    StaticField(NAME(HYP_STR(KEY_0)), KeyCode::KEY_0),
    StaticField(NAME(HYP_STR(KEY_1)), KeyCode::KEY_1),
    StaticField(NAME(HYP_STR(KEY_2)), KeyCode::KEY_2),
    StaticField(NAME(HYP_STR(KEY_3)), KeyCode::KEY_3),
    StaticField(NAME(HYP_STR(KEY_4)), KeyCode::KEY_4),
    StaticField(NAME(HYP_STR(KEY_5)), KeyCode::KEY_5),
    StaticField(NAME(HYP_STR(KEY_6)), KeyCode::KEY_6),
    StaticField(NAME(HYP_STR(KEY_7)), KeyCode::KEY_7),
    StaticField(NAME(HYP_STR(KEY_8)), KeyCode::KEY_8),
    StaticField(NAME(HYP_STR(KEY_9)), KeyCode::KEY_9),
    StaticField(NAME(HYP_STR(KEY_F1)), KeyCode::KEY_F1),
    StaticField(NAME(HYP_STR(KEY_F2)), KeyCode::KEY_F2),
    StaticField(NAME(HYP_STR(KEY_F3)), KeyCode::KEY_F3),
    StaticField(NAME(HYP_STR(KEY_F4)), KeyCode::KEY_F4),
    StaticField(NAME(HYP_STR(KEY_F5)), KeyCode::KEY_F5),
    StaticField(NAME(HYP_STR(KEY_F6)), KeyCode::KEY_F6),
    StaticField(NAME(HYP_STR(KEY_F7)), KeyCode::KEY_F7),
    StaticField(NAME(HYP_STR(KEY_F8)), KeyCode::KEY_F8),
    StaticField(NAME(HYP_STR(KEY_F9)), KeyCode::KEY_F9),
    StaticField(NAME(HYP_STR(KEY_F10)), KeyCode::KEY_F10),
    StaticField(NAME(HYP_STR(KEY_F11)), KeyCode::KEY_F11),
    StaticField(NAME(HYP_STR(KEY_F12)), KeyCode::KEY_F12),
    StaticField(NAME(HYP_STR(KEY_LCTRL)), KeyCode::KEY_LCTRL),
    StaticField(NAME(HYP_STR(KEY_LSHIFT)), KeyCode::KEY_LSHIFT),
    StaticField(NAME(HYP_STR(KEY_LALT)), KeyCode::KEY_LALT),
    StaticField(NAME(HYP_STR(KEY_RCTRL)), KeyCode::KEY_RCTRL),
    StaticField(NAME(HYP_STR(KEY_RSHIFT)), KeyCode::KEY_RSHIFT),
    StaticField(NAME(HYP_STR(KEY_RALT)), KeyCode::KEY_RALT),
    StaticField(NAME(HYP_STR(KEY_SPACE)), KeyCode::KEY_SPACE),
    StaticField(NAME(HYP_STR(KEY_COMMA)), KeyCode::KEY_COMMA),
    StaticField(NAME(HYP_STR(KEY_DASH)), KeyCode::KEY_DASH),
    StaticField(NAME(HYP_STR(KEY_PERIOD)), KeyCode::KEY_PERIOD),
    StaticField(NAME(HYP_STR(KEY_RETURN)), KeyCode::KEY_RETURN),
    StaticField(NAME(HYP_STR(KEY_TAB)), KeyCode::KEY_TAB),
    StaticField(NAME(HYP_STR(KEY_BACKSPACE)), KeyCode::KEY_BACKSPACE),
    StaticField(NAME(HYP_STR(KEY_CAPSLOCK)), KeyCode::KEY_CAPSLOCK),
    StaticField(NAME(HYP_STR(KEY_TILDE)), KeyCode::KEY_TILDE),
    StaticField(NAME(HYP_STR(KEY_RIGHT)), KeyCode::KEY_RIGHT),
    StaticField(NAME(HYP_STR(KEY_LEFT)), KeyCode::KEY_LEFT),
    StaticField(NAME(HYP_STR(KEY_DOWN)), KeyCode::KEY_DOWN),
    StaticField(NAME(HYP_STR(KEY_UP)), KeyCode::KEY_UP),
    StaticField(NAME(HYP_STR(KEY_ESCAPE)), KeyCode::KEY_ESCAPE)
HYP_END_ENUM

#pragma endregion KeyCode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region KeyboardEvent Reflection Data

HYP_BEGIN_STRUCT(KeyboardEvent, 276, 0, {}, ClassAttribute("size", 16))
    Field(NAME(HYP_STR(InputManager)), &KeyboardEvent::inputManager, offsetof(KeyboardEvent, inputManager)),
    Field(NAME(HYP_STR(KeyCode)), &KeyboardEvent::keyCode, offsetof(KeyboardEvent, keyCode))
HYP_END_STRUCT

#pragma endregion KeyboardEvent Reflection Data

static_assert(sizeof(KeyboardEvent) == 16, "Expected sizeof(KeyboardEvent) to be 16 bytes");
} // namespace hyperion

