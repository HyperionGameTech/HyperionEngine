/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <Rendering/dx12/DX12Sampler.hpp>
#include <Rendering/dx12/DX12RenderInterface.hpp>
#include <Rendering/dx12/DX12Helpers.hpp>

#include <DX12Sampler.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

#pragma region DX12Sampler

DX12Sampler::DX12Sampler(const SamplerDesc& desc)
    : SamplerBase(desc),
      m_samplerDesc {},
      m_isCreated(false)
{
}

DX12Sampler::~DX12Sampler()
{
}

bool DX12Sampler::IsCreated() const
{
    return m_isCreated;
}

RendererResult DX12Sampler::Create()
{
    if (m_isCreated)
    {
        return HYP_MAKE_ERROR(RendererError, "Sampler already created");
    }

    m_samplerDesc = GetSamplerDesc(this);
    m_isCreated = true;

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
