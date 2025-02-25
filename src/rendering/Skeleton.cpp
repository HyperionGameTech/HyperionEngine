/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Skeleton.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region SkeletonRenderResource

SkeletonRenderResource::SkeletonRenderResource(Skeleton *skeleton)
    : m_skeleton(skeleton),
      m_buffer_data { }
{
}

SkeletonRenderResource::SkeletonRenderResource(SkeletonRenderResource &&other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase &&>(other)),
      m_skeleton(other.m_skeleton),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_skeleton = nullptr;
}

SkeletonRenderResource::~SkeletonRenderResource() = default;

void SkeletonRenderResource::Initialize_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_skeleton != nullptr);
    
    UpdateBufferData();
}

void SkeletonRenderResource::Destroy_Internal()
{
    HYP_SCOPE;
}

void SkeletonRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *SkeletonRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->skeletons.Get();
}

void SkeletonRenderResource::UpdateBufferData()
{
    HYP_SCOPE;

    AssertThrow(m_buffer_index != ~0u);

    *static_cast<SkeletonShaderData *>(m_buffer_address) = m_buffer_data;
    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void SkeletonRenderResource::SetBufferData(const SkeletonShaderData &buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
    {
        m_buffer_data = buffer_data;

        if (m_is_initialized) {
            UpdateBufferData();
        }
    });
}

#pragma endregion SkeletonRenderResource

namespace renderer {

HYP_DESCRIPTOR_SSBO(Object, SkeletonsBuffer, 1, sizeof(SkeletonShaderData), true);

} // namespace renderer
} // namespace hyperion