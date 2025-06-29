/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_SKELETON_HPP
#define HYPERION_RENDERING_SKELETON_HPP

#include <rendering/RenderResource.hpp>

#include <core/math/Matrix4.hpp>

#include <Types.hpp>

namespace hyperion {

class Skeleton;

struct SkeletonShaderData
{
    static constexpr SizeType max_bones = 256;

    Matrix4 bones[max_bones];
};

class RenderSkeleton final : public RenderResourceBase
{
public:
    RenderSkeleton(Skeleton* skeleton);
    RenderSkeleton(RenderSkeleton&& other) noexcept;
    virtual ~RenderSkeleton() override;

    void SetBufferData(const SkeletonShaderData& buffer_data);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GpuBufferHolderBase* GetGpuBufferHolder() const override;

private:
    void UpdateBufferData();

    Skeleton* m_skeleton;
    SkeletonShaderData m_buffer_data;
};

} // namespace hyperion

#endif