/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/CullData.hpp>
#include <rendering/SafeDeleter.hpp>

namespace hyperion {

CullData::CullData(const CullData& other)
    : depth_pyramid_image_view(other.depth_pyramid_image_view),
      depth_pyramid_dimensions(other.depth_pyramid_dimensions)
{
}

CullData& CullData::operator=(const CullData& other)
{
    if (this == &other)
    {
        return *this;
    }

    if (depth_pyramid_image_view != other.depth_pyramid_image_view)
    {
        SafeRelease(std::move(depth_pyramid_image_view));

        depth_pyramid_image_view = other.depth_pyramid_image_view;
    }

    depth_pyramid_dimensions = other.depth_pyramid_dimensions;

    return *this;
}

CullData::CullData(CullData&& other) noexcept
    : depth_pyramid_image_view(std::move(other.depth_pyramid_image_view)),
      depth_pyramid_dimensions(other.depth_pyramid_dimensions)
{
    other.depth_pyramid_dimensions = Vec2u::One();
}

CullData& CullData::operator=(CullData&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (depth_pyramid_image_view != other.depth_pyramid_image_view)
    {
        SafeRelease(std::move(depth_pyramid_image_view));

        depth_pyramid_image_view = std::move(other.depth_pyramid_image_view);
    }

    depth_pyramid_dimensions = other.depth_pyramid_dimensions;
    other.depth_pyramid_dimensions = Vec2u::One();

    return *this;
}

CullData::~CullData()
{
    SafeRelease(std::move(depth_pyramid_image_view));
}

} // namespace hyperion