/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_INPUT_HANDLER_HPP
#define HYPERION_INPUT_HANDLER_HPP

#include <input/Keyboard.hpp>
#include <input/Mouse.hpp>

#include <core/math/Vector2.hpp>

#include <core/object/HypObject.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

HYP_CLASS(Abstract)
class HYP_API InputHandlerBase : public EnableRefCountedPtrFromThis<InputHandlerBase>
{
    HYP_OBJECT_BODY(InputHandlerBase);

public:
    InputHandlerBase()          = default;
    virtual ~InputHandlerBase() = default;

    HYP_METHOD(Scriptable)
    bool OnKeyDown(const KeyboardEvent &evt);

    HYP_METHOD(Scriptable)
    bool OnKeyUp(const KeyboardEvent &evt);

    HYP_METHOD(Scriptable)
    bool OnMouseDown(const MouseEvent &evt);

    HYP_METHOD(Scriptable)
    bool OnMouseUp(const MouseEvent &evt);

    HYP_METHOD(Scriptable)
    bool OnMouseMove(const MouseEvent &evt);

    HYP_METHOD(Scriptable)
    bool OnMouseDrag(const MouseEvent &evt);

    HYP_METHOD(Scriptable)
    bool OnClick(const MouseEvent &evt);

    HYP_METHOD()
    virtual bool OnKeyDown_Impl(const KeyboardEvent &evt) = 0;

    HYP_METHOD()
    virtual bool OnKeyUp_Impl(const KeyboardEvent &evt) = 0;

    HYP_METHOD()
    virtual bool OnMouseDown_Impl(const MouseEvent &evt) = 0;

    HYP_METHOD()
    virtual bool OnMouseUp_Impl(const MouseEvent &evt) = 0;

    HYP_METHOD()
    virtual bool OnMouseMove_Impl(const MouseEvent &evt) = 0;

    HYP_METHOD()
    virtual bool OnMouseDrag_Impl(const MouseEvent &evt) = 0;

    HYP_METHOD()
    virtual bool OnClick_Impl(const MouseEvent &evt) = 0;
};

HYP_CLASS()
class NullInputHandler final : public InputHandlerBase
{
    HYP_OBJECT_BODY(NullInputHandler);

public:
    NullInputHandler()                      = default;
    virtual ~NullInputHandler() override    = default;

protected:
    virtual bool OnKeyDown_Impl(const KeyboardEvent &evt) override    { return false; }
    virtual bool OnKeyUp_Impl(const KeyboardEvent &evt) override      { return false; }
    virtual bool OnMouseDown_Impl(const MouseEvent &evt) override     { return false; }
    virtual bool OnMouseUp_Impl(const MouseEvent &evt) override       { return false; }
    virtual bool OnMouseMove_Impl(const MouseEvent &evt) override     { return false; }
    virtual bool OnMouseDrag_Impl(const MouseEvent &evt) override     { return false; }
    virtual bool OnClick_Impl(const MouseEvent &evt) override         { return false; }
};

} // namespace hyperion

#endif