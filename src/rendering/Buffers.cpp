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

void GPUBufferHolderBase::CreateBuffers(SizeType count, SizeType size, SizeType alignment)
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        DeferCreate(m_buffers[frame_index], size * count, alignment);
    }
}

#pragma endregion GPUBufferHolderBase

} // namespace hyperion