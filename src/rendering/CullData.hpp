/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CULL_DATA_HPP
#define HYPERION_CULL_DATA_HPP

#include <core/math/Extent.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

struct CullData
{
    ImageViewRef depth_pyramid_image_view;
    Vec2u depth_pyramid_dimensions;

    CullData()
        : depth_pyramid_dimensions(Vec2u::One())
    {
    }

    CullData(const CullData& other);
    CullData& operator=(const CullData& other);

    CullData(CullData&& other) noexcept;
    CullData& operator=(CullData&& other) noexcept;

    ~CullData();

    HYP_FORCE_INLINE bool operator==(const CullData& other) const
    {
        return depth_pyramid_image_view == other.depth_pyramid_image_view
            && depth_pyramid_dimensions == other.depth_pyramid_dimensions;
    }

    HYP_FORCE_INLINE bool operator!=(const CullData& other) const
    {
        return depth_pyramid_image_view != other.depth_pyramid_image_view
            || depth_pyramid_dimensions != other.depth_pyramid_dimensions;
    }
};

} // namespace hyperion

#endif