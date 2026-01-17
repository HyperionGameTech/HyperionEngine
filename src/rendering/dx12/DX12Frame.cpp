/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Frame.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12Frame.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12Frame

DX12Frame::DX12Frame()
    : FrameBase(0)
{
}

DX12Frame::DX12Frame(uint32 frameIndex)
    : FrameBase(frameIndex)
{
}

DX12Frame::~DX12Frame()
{
}

bool DX12Frame::IsCreated() const
{
    return false;
}

RendererResult DX12Frame::Create()
{
    // @TODO
    return {};
}

void DX12Frame::OnFrameStart()
{
    // @TODO
}

#pragma endregion DX12Frame

} // namespace Hyperion
