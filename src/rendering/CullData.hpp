#ifndef HYPERION_V2_CULL_DATA_H
#define HYPERION_V2_CULL_DATA_H

#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <core/lib/FixedArray.hpp>

#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::ImageView;

struct CullData
{
    FixedArray<const ImageView *, max_frames_in_flight> depth_pyramid_image_views;
    Extent3D depth_pyramid_dimensions;

    CullData()
        : depth_pyramid_image_views { nullptr },
          depth_pyramid_dimensions { 1, 1, 1 }
    {
    }

    bool operator==(const CullData &other) const
    {
        return depth_pyramid_image_views[0] == other.depth_pyramid_image_views[0]
            && depth_pyramid_image_views[1] == other.depth_pyramid_image_views[1]
            && depth_pyramid_dimensions == other.depth_pyramid_dimensions;
    }

    bool operator!=(const CullData &other) const
    {
        return !operator==(other);
    }
};

} // namespace hyperion::v2

#endif