#pragma once

#include <Core/FxSizedArray.hpp>
#include <Core/FxTypes.hpp>
#include <Math/FxVec2.hpp>
#include <Util/FxKey.hpp>
#include <cstring>

// Forward Declarations
union SDL_Event;
class FxControl;

class FxControlManager
{
private:
    friend void CheckKeyForContinuedPress(FxControl* key);

public:
    static const uint32 scMaxKeys = static_cast<uint32>(FxKey::FX_KEY_MAX);

    FxControlManager() = default;

    static void Init();

    static FxControlManager& GetInstance();

    static void CaptureMouse();
    static void ReleaseMouse();
    static bool IsMouseLocked();

    static FxVec2f& GetMouseDelta();

    /**
     * Check if a key is currently pressed.
     */
    static bool IsKeyDown(FxKey scancode);

    /**
     * Check if a key is currently released.
     */
    static bool IsKeyUp(FxKey scancode);

    /**
     * Check if a key was pressed once.
     *
     * This returns `true` if the key was pressed for the first time
     * during this frame. It will return `false` if the key was already pressed
     * before this frame or if it was released during this frame.
     *
     * - Any occurances of this function called during the frame will return true.
     *
     * *NOTE:* The current "scope" for currently pressed keys is reset or begun on
     * a call to `FxControlManager::Update`. As well, if this function
     * is not called within the bounds of *every* `::Update` call, then
     * the bit that determines if the key is on the next frame returns false positives.
     * This *ONLY* pertains to if the key is held down.
     */
    static bool IsKeyPressed(FxKey scancode);

    /**
     * Checks if a combination of keys is currently pressed down.
     */
    template <std::same_as<FxKey>... KeyV>
    static bool IsComboDown(KeyV... keys)
    {
        return (IsKeyDown(keys) && ...);
    }

    /**
     * Checks if a combination of keys is no longer pressed.
     */
    template <std::same_as<FxKey>... KeyV>
    static bool IsComboUp(KeyV... keys)
    {
        return (IsKeyUp(keys) && ...);
    }

    /**
     * Checks if a combination of keys have just been pressed down.
     *
     * This function will only return true if at least one of the keys satisifies `IsKeyPressed()`,
     * and all of the keys are pressed down.
     */
    template <std::same_as<FxKey>... KeyV>
    static bool IsComboPressed(KeyV... keys)
    {
        // If all keys are down and at least one key has just been pressed, then return true
        return (IsKeyDown(keys) && ...) && (IsKeyPressed(keys) || ...);
    }

    /**
     * Get a pointer to the control associated with the given keycode.
     * Returns `nullptr` if the keycode does not exist or is out of range.
     */
    static FxControl* GetKey(FxKey scancode);

    static void ResetKey(FxKey scancode);

    ////////////////////////////////
    // Update functions
    ////////////////////////////////

    static void Update();

private:
    static void UpdateFromKeyboardEvent(SDL_Event* event);
    static void UpdateFromMouseButtonEvent(SDL_Event* event);
    static void UpdateFromMouseMoveEvent(SDL_Event* event);

    // General, used by UpdateFromKeyboardEvent and UpdateFromMouseButtonEvent
    static void UpdateButtonFromEvent(FxKey key_id, bool is_now_down);

public:
    using WindowEventFunc = void (*)();

    WindowEventFunc OnQuit;

private:
    FxSizedArray<FxControl> mKeyMap;
    bool mMouseCaptured = false;

    FxVec2f mMouseDelta = FxVec2f::sZero;
    FxVec2f mCapturedMousePos = FxVec2f::sZero;

    uint8 mThisTick : 1 = 0;
};

class FxControl
{
private:
    friend void CheckKeyForContinuedPress(FxControl* control);
    friend class FxControlManager;

public:
public:
    inline bool IsKeyDown() const { return mbKeyDown; }
    inline bool IsKeyUp() const { return !mbKeyDown; }

    inline bool IsContinuedPress() const { return mbContinuedPress; }

    inline void Reset()
    {
        mbContinuedPress = false;
        mbKeyDown = false;
    }

    bool IsKeyPressed() const { return (IsKeyDown() && !IsContinuedPress()); }

private:
    uint8 mbContinuedPress : 1 = 0;
    uint8 mbKeyDown : 1 = 0;
    uint8 mbTickBit : 1 = 0;
};
