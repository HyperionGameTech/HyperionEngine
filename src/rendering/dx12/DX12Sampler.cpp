/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Sampler.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12Sampler.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12Sampler

DX12Sampler::DX12Sampler(
    TextureFilterMode minFilterMode,
    TextureFilterMode magFilterMode,
    TextureWrapMode wrapMode)
    : SamplerBase()
{
    m_minFilterMode = minFilterMode;
    m_magFilterMode = magFilterMode;
    m_wrapMode = wrapMode;
}

DX12Sampler::~DX12Sampler()
{
}

bool DX12Sampler::IsCreated() const
{
    return false;
}

RendererResult DX12Sampler::Create()
{
    // @TODO
    HYPERION_RETURN_OK;
}

#ifdef HYP_DEBUG_MODE
void DX12Sampler::SetDebugName(Name name)
{
    SamplerBase::SetDebugName(name);
}
#endif

#pragma endregion DX12Sampler

} // namespace Hyperion
