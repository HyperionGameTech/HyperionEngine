/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/math/Extent.hpp>

#include <Core/reflection/Handle.hpp>

#include <rendering/RenderObject.hpp>

namespace Hyperion {

struct CullData
{
    GpuImageViewRef depthPyramidImageView;
    Vec2u depthPyramidDimensions;

    CullData()
        : depthPyramidDimensions(Vec2u::One())
    {
    }

    CullData(const CullData& other);
    CullData& operator=(const CullData& other);

    CullData(CullData&& other) noexcept;
    CullData& operator=(CullData&& other) noexcept;

    ~CullData();

    HYP_FORCE_INLINE bool operator==(const CullData& other) const
    {
        return depthPyramidImageView == other.depthPyramidImageView
            && depthPyramidDimensions == other.depthPyramidDimensions;
    }

    HYP_FORCE_INLINE bool operator!=(const CullData& other) const
    {
        return depthPyramidImageView != other.depthPyramidImageView
            || depthPyramidDimensions != other.depthPyramidDimensions;
    }
};

} // namespace Hyperion
