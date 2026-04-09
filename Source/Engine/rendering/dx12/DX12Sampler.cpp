/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Sampler.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>

#include <DX12Sampler.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12Sampler

DX12Sampler::DX12Sampler(const SamplerDesc& desc)
    : SamplerBase(desc)
{
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
    return {};
}

#ifdef HYP_DEBUG_MODE
void DX12Sampler::SetDebugName(Name name)
{
    SamplerBase::SetDebugName(name);
}
#endif

#pragma endregion DX12Sampler

} // namespace Hyperion
