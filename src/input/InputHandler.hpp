/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <input/Keyboard.hpp>
#include <input/Mouse.hpp>

#include <core/math/Vector2.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/memory/Pimpl.hpp>

namespace hyperion {

struct InputState;

HYP_CLASS(Abstract)
class HYP_API InputHandlerBase : public ObjectBase
{
    HYP_OBJECT_BODY(InputHandlerBase);

public:
    InputHandlerBase();
    InputHandlerBase(const InputHandlerBase& other) = delete;
    InputHandlerBase& operator=(const InputHandlerBase& other) = delete;
    InputHandlerBase(InputHandlerBase&& other) noexcept = delete;
    InputHandlerBase& operator=(InputHandlerBase&& other) noexcept = delete;
    virtual ~InputHandlerBase();

    HYP_FORCE_INLINE const Bitset& GetKeyStates() const
    {
        return m_keyStates;
    }

    HYP_FORCE_INLINE EnumFlags<MouseButtonState> GetMouseButtonStates() const
    {
        return m_mouseButtonStates;
    }

    HYP_FORCE_INLINE void SetDeltaTime(double deltaTime)
    {
        m_deltaTime = deltaTime;
    }

    HYP_METHOD(Scriptable)
    bool OnKeyDown(const KeyboardEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnKeyUp(const KeyboardEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnMouseDown(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnMouseUp(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnMouseMove(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnMouseDrag(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnMouseLeave(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnClick(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnGainFocus(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnLoseFocus(const MouseEvent& evt);

    bool IsKeyDown(KeyCode key) const;
    bool IsKeyUp(KeyCode key) const;

    bool IsMouseButtonDown(MouseButtonKey btn) const;
    bool IsMouseButtonUp(MouseButtonKey btn) const;

protected:
    HYP_METHOD()
    virtual bool OnKeyDown_Impl(const KeyboardEvent& evt);

    HYP_METHOD()
    virtual bool OnKeyUp_Impl(const KeyboardEvent& evt);

    HYP_METHOD()
    virtual bool OnMouseDown_Impl(const MouseEvent& evt);

    HYP_METHOD()
    virtual bool OnMouseUp_Impl(const MouseEvent& evt);

    HYP_METHOD()
    virtual bool OnMouseMove_Impl(const MouseEvent& evt) = 0;

    HYP_METHOD()
    virtual bool OnMouseDrag_Impl(const MouseEvent& evt) = 0;

    HYP_METHOD()
    virtual bool OnMouseLeave_Impl(const MouseEvent& evt) = 0;

    HYP_METHOD()
    virtual bool OnClick_Impl(const MouseEvent& evt) = 0;

    HYP_METHOD()
    virtual bool OnGainFocus_Impl(const MouseEvent& evt) = 0;

    HYP_METHOD()
    virtual bool OnLoseFocus_Impl(const MouseEvent& evt) = 0;

private:
    Bitset m_keyStates;
    EnumFlags<MouseButtonState> m_mouseButtonStates;

protected:
    double m_deltaTime;
};

HYP_CLASS()
class NullInputHandler final : public InputHandlerBase
{
    HYP_OBJECT_BODY(NullInputHandler);

public:
    NullInputHandler() = default;
    virtual ~NullInputHandler() override = default;

private:
    void Init() override
    {
        SetReady(true);
    }

    virtual bool OnKeyDown_Impl(const KeyboardEvent& evt) override
    {
        return false;
    }

    virtual bool OnKeyUp_Impl(const KeyboardEvent& evt) override
    {
        return false;
    }

    virtual bool OnMouseDown_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnMouseUp_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnMouseMove_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnMouseDrag_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnMouseLeave_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnClick_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnGainFocus_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnLoseFocus_Impl(const MouseEvent& evt) override
    {
        return false;
    }
};

} // namespace hyperion
