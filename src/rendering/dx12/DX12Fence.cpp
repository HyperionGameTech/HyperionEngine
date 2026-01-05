/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Fence.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12Fence.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

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
    HYPERION_RETURN_OK;
}

RendererResult DX12Fence::Wait(bool timeoutLoop)
{
    // @TODO
    HYPERION_RETURN_OK;
}

RendererResult DX12Fence::Reset()
{
    // @TODO
    HYPERION_RETURN_OK;
}

#pragma endregion DX12Fence

} // namespace Hyperion
