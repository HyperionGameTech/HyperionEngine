/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12CommandBuffer.hpp>

#include <DX12CommandBuffer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderBackend* g_renderBackend;

DX12CommandBuffer::DX12CommandBuffer()
{
}

DX12CommandBuffer::~DX12CommandBuffer()
{
}

} // namespace Hyperion
