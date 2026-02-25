/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/camera/OrthoCamera.hpp>

namespace Hyperion {

HYP_CLASS()
class UICameraController : public OrthoCameraController
{
    HYP_OBJECT_BODY(UICameraController);

public:
    UICameraController() = default;
    UICameraController(float left, float right, float bottom, float top, float _near, float _far)
        : OrthoCameraController(left, right, bottom, top, _near, _far)
    {
    }

    virtual ~UICameraController() override = default;
};

} // namespace Hyperion
