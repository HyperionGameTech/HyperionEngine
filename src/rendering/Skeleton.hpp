/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_SKELETON_HPP
#define HYPERION_RENDERING_SKELETON_HPP

#include <math/Matrix4.hpp>

#include <Types.hpp>

namespace hyperion {

struct alignas(256) SkeletonShaderData
{
    static constexpr SizeType max_bones = 256;

    Matrix4 bones[max_bones];
};

static_assert(sizeof(SkeletonShaderData) % 256 == 0);

static constexpr uint32 max_skeletons = (8ull * 1024ull * 1024ull) / sizeof(SkeletonShaderData);

} // namespace hyperion

#endif