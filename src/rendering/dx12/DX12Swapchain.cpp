/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Swapchain.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12Swapchain.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12Swapchain

DX12Swapchain::DX12Swapchain(const Vec2u& extent)
    : SwapchainBase(extent)
{
}

DX12Swapchain::~DX12Swapchain()
{
}

bool DX12Swapchain::IsCreated() const
{
    return false;
}

RendererResult DX12Swapchain::Create()
{
    // @TODO
    return {};
}

void DX12Swapchain::SetExtent(Vec2u newExtent)
{
    m_extent = newExtent;
}

void DX12Swapchain::Recreate()
{
    // @TODO
}

#pragma endregion DX12Swapchain

} // namespace Hyperion
