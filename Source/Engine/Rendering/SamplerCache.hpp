/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/Pimpl.hpp>

#include <Core/HashCode.hpp>

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

struct SamplerDesc;

class SamplerCache
{
public:
    SamplerCache();

    SamplerCache(const SamplerCache&) = delete;
    SamplerCache& operator=(const SamplerCache&) = delete;

    SamplerCache(SamplerCache&&) = delete;
    SamplerCache& operator=(SamplerCache&&) = delete;

    ~SamplerCache() = default;

    Sampler* GetOrCreate(const SamplerDesc& samplerDesc);

private:
    Pimpl<class SamplerCacheImpl> m_impl;
};

} // namespace Hyperion
