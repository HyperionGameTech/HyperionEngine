/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Buffers.hpp>

#include <rendering/backend/RenderingAPI.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region GPUBufferHolderBase

GPUBufferHolderBase::~GPUBufferHolderBase()
{
    SafeRelease(std::move(m_buffers));
}

void GPUBufferHolderBase::CreateBuffers(GPUBufferType type, SizeType initial_count, SizeType size, SizeType alignment)
{
    if (initial_count == 0)
    {
        initial_count = 1;
    }

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        m_buffers[frame_index] = g_rendering_api->MakeGPUBuffer(type, size * initial_count, alignment);

        DeferCreate(m_buffers[frame_index]);
    }
}

#pragma endregion GPUBufferHolderBase

} // namespace hyperion