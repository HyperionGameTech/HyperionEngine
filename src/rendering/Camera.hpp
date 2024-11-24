/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_CAMERA_HPP
#define HYPERION_RENDERING_CAMERA_HPP

#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>

#include <Types.hpp>

namespace hyperion {

struct alignas(256) CameraShaderData
{
    Matrix4     view;
    Matrix4     projection;
    Matrix4     previous_view;

    Vec4u       dimensions;
    Vec4f       camera_position;
    Vec4f       camera_direction;
    Vec4f       jitter;
    
    float       camera_near;
    float       camera_far;
    float       camera_fov;
    float       _pad0;
};

static_assert(sizeof(CameraShaderData) == 512);

static constexpr uint32 max_cameras = (16ull * 1024ull) / sizeof(CameraShaderData);

} // namespace hyperion

#endif
