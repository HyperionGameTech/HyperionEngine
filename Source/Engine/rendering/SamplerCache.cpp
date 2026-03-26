/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/SamplerCache.hpp>
#include <rendering/Sampler.hpp>
#include <rendering/RenderInterface.hpp>

#include <Core/containers/HashMap.hpp>

#include <Core/threading/SharedMutex.hpp>

#include <engine/EngineGlobals.hpp>

namespace Hyperion {

#pragma region SamplerCacheImpl

class SamplerCacheImpl
{
public:
    HashMap<SamplerDesc, SamplerRef> cache;
    SharedMutex mutex;
};

#pragma endregion SamplerCacheImpl

SamplerCache::SamplerCache()
    : m_impl(MakePimpl<SamplerCacheImpl>())
{
}

const SamplerRef& SamplerCache::GetOrCreate(const SamplerDesc& samplerDesc)
{
    TSharedLock lock(m_impl->mutex);

    auto it = m_impl->cache.Find(samplerDesc);

    if (it != m_impl->cache.End())
    {
        return it->second;
    }

    lock.Reset();

    TUniqueLock uniqueLock(m_impl->mutex);

    // Check again in case another thread created the sampler
    it = m_impl->cache.Find(samplerDesc);

    if (it != m_impl->cache.End())
    {
        return it->second;
    }

    SamplerRef sampler = g_renderInterface->MakeSampler(samplerDesc);
    CheckResult(sampler->Create());

    m_impl->cache[samplerDesc] = sampler;

    return m_impl->cache[samplerDesc];
}

} // namespace Hyperion
