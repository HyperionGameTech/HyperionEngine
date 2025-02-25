/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Skeleton.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region SkeletonRenderResources

SkeletonRenderResources::SkeletonRenderResources(Skeleton *skeleton)
    : m_skeleton(skeleton),
      m_buffer_data { }
{
}

SkeletonRenderResources::SkeletonRenderResources(SkeletonRenderResources &&other) noexcept
    : RenderResourcesBase(static_cast<RenderResourcesBase &&>(other)),
      m_skeleton(other.m_skeleton),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_skeleton = nullptr;
}

SkeletonRenderResources::~SkeletonRenderResources() = default;

void SkeletonRenderResources::Initialize_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_skeleton != nullptr);
    
    UpdateBufferData();
}

void SkeletonRenderResources::Destroy_Internal()
{
    HYP_SCOPE;
}

void SkeletonRenderResources::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *SkeletonRenderResources::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->skeletons.Get();
}

void SkeletonRenderResources::UpdateBufferData()
{
    HYP_SCOPE;

    AssertThrow(m_buffer_index != ~0u);

    *static_cast<SkeletonShaderData *>(m_buffer_address) = m_buffer_data;
    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void SkeletonRenderResources::SetBufferData(const SkeletonShaderData &buffer_data)
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

#pragma endregion SkeletonRenderResources

namespace renderer {

HYP_DESCRIPTOR_SSBO(Object, SkeletonsBuffer, 1, sizeof(SkeletonShaderData), true);

} // namespace renderer
} // namespace hyperion