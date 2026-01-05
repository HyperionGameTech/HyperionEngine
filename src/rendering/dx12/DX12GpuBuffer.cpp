/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuBuffer.hpp>

#include <DX12GpuBuffer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderBackend* g_renderBackend;

DX12GpuBuffer::DX12GpuBuffer(GpuBufferType type, SizeType size, SizeType alignment)
    : GpuBufferBase(type, size, alignment)
{
}

DX12GpuBuffer::~DX12GpuBuffer()
{
    // @TODO
}



} // namespace Hyperion
