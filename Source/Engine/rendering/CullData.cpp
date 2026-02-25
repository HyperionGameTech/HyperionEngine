/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/CullData.hpp>
#include <rendering/GpuImageView.hpp>
#include <rendering/GpuBuffer.hpp>

#include <rendering/util/DeletionQueue.hpp>

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
