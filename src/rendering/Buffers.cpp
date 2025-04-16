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

void GPUBufferHolderBase::CreateBuffers(GPUBufferType type, SizeType count, SizeType size, SizeType alignment)
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_buffers[frame_index] = g_rendering_api->MakeGPUBuffer(type, size * count, alignment);

        DeferCreate(m_buffers[frame_index]);
    }
}

#pragma endregion GPUBufferHolderBase

} // namespace hyperion