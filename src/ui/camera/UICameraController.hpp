/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/camera/OrthoCamera.hpp>

namespace hyperion {

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

} // namespace hyperion
