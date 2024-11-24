/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_SCENE_HPP
#define HYPERION_RENDERING_SCENE_HPP

#include <math/Vector4.hpp>

#include <Types.hpp>

namespace hyperion {

struct SceneShaderData
{
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4f   fog_params;

    float   game_time;
    uint32  frame_counter;
    uint32  enabled_render_components_mask;
    uint32  enabled_environment_maps_mask;

    HYP_PAD_STRUCT_HERE(uint8, 64 + 128);
};

static_assert(sizeof(SceneShaderData) == 256);

static constexpr uint32 max_scenes = (32ull * 1024ull) / sizeof(SceneShaderData);

} // namespace hyperion

#endif
