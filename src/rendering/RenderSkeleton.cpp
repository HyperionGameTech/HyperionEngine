/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderSkeleton.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderSkeleton

RenderSkeleton::RenderSkeleton(Skeleton* skeleton)
    : m_skeleton(skeleton),
      m_buffer_data {}
{
}

RenderSkeleton::RenderSkeleton(RenderSkeleton&& other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase&&>(other)),
      m_skeleton(other.m_skeleton),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_skeleton = nullptr;
}

RenderSkeleton::~RenderSkeleton() = default;

void RenderSkeleton::Initialize_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_skeleton != nullptr);

    UpdateBufferData();
}

void RenderSkeleton::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderSkeleton::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase* RenderSkeleton::GetGPUBufferHolder() const
{
    return g_render_global_state->Skeletons;
}

void RenderSkeleton::UpdateBufferData()
{
    HYP_SCOPE;

    AssertThrow(m_buffer_index != ~0u);

    *static_cast<SkeletonShaderData*>(m_buffer_address) = m_buffer_data;
    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void RenderSkeleton::SetBufferData(const SkeletonShaderData& buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
        {
            m_buffer_data = buffer_data;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

#pragma endregion RenderSkeleton

namespace renderer {

HYP_DESCRIPTOR_SSBO(Object, SkeletonsBuffer, 1, sizeof(SkeletonShaderData), true);

} // namespace renderer
} // namespace hyperion