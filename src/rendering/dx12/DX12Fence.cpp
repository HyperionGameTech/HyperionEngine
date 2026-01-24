/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Fence.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>

#include <DX12Fence.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12Fence

DX12Fence::DX12Fence()
{
}

DX12Fence::~DX12Fence()
{
}

RendererResult DX12Fence::Create()
{
    // @TODO
    return {};
}

RendererResult DX12Fence::Wait(bool timeoutLoop)
{
    // @TODO
    return {};
}

RendererResult DX12Fence::Reset()
{
    // @TODO
    return {};
}

#pragma endregion DX12Fence

} // namespace Hyperion
