/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_SKELETON_HPP
#define HYPERION_RENDERING_SKELETON_HPP

#include <rendering/RenderResource.hpp>

#include <core/math/Matrix4.hpp>

#include <Types.hpp>

namespace hyperion {

class Skeleton;

struct alignas(256) SkeletonShaderData
{
    static constexpr SizeType max_bones = 256;

    Matrix4 bones[max_bones];
};

static_assert(sizeof(SkeletonShaderData) % 256 == 0);

static constexpr uint32 max_skeletons = (8ull * 1024ull * 1024ull) / sizeof(SkeletonShaderData);

class SkeletonRenderResource final : public RenderResourceBase
{
public:
    SkeletonRenderResource(Skeleton *skeleton);
    SkeletonRenderResource(SkeletonRenderResource &&other) noexcept;
    virtual ~SkeletonRenderResource() override;

    void SetBufferData(const SkeletonShaderData &buffer_data);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;
    
    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

    virtual Name GetTypeName() const override
        { return NAME("SkeletonRenderResource"); }

private:
    void UpdateBufferData();

    Skeleton            *m_skeleton;
    SkeletonShaderData  m_buffer_data;
};

} // namespace hyperion

#endif