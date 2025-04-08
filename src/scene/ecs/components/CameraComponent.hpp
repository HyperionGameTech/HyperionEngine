/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_CAMERA_COMPONENT_HPP
#define HYPERION_ECS_CAMERA_COMPONENT_HPP

#include <core/Handle.hpp>

#include <HashCode.hpp>

namespace hyperion {

class CameraController;
class Camera;

HYP_STRUCT(Component, Label="Camera Component", Size=8)
struct CameraComponent
{
    HYP_FIELD(Property="Camera", Serialize=true, Editor=true)
    Handle<Camera>  camera;
};

} // namespace hyperion

#endif