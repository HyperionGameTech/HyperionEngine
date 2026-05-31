/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/CullData.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/GpuBuffer.hpp>

#include <Rendering/util/DeletionQueue.hpp>

namespace Hyperion {

CullData::CullData(const CullData& other)
    : depthPyramidImageView(other.depthPyramidImageView),
      depthPyramidDimensions(other.depthPyramidDimensions)
{
}

CullData& CullData::operator=(const CullData& other)
{
    if (this == &other)
    {
        return *this;
    }

    if (depthPyramidImageView != other.depthPyramidImageView)
    {
        EnqueueDeletion(std::move(depthPyramidImageView));

        depthPyramidImageView = other.depthPyramidImageView;
    }

    depthPyramidDimensions = other.depthPyramidDimensions;

    return *this;
}

CullData::CullData(CullData&& other) noexcept
    : depthPyramidImageView(std::move(other.depthPyramidImageView)),
      depthPyramidDimensions(other.depthPyramidDimensions)
{
    other.depthPyramidDimensions = Vec2u::One();
}

CullData& CullData::operator=(CullData&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (depthPyramidImageView != other.depthPyramidImageView)
    {
        EnqueueDeletion(std::move(depthPyramidImageView));

        depthPyramidImageView = std::move(other.depthPyramidImageView);
    }

    depthPyramidDimensions = other.depthPyramidDimensions;
    other.depthPyramidDimensions = Vec2u::One();

    return *this;
}

CullData::~CullData()
{
    EnqueueDeletion(std::move(depthPyramidImageView));
}

} // namespace Hyperion
