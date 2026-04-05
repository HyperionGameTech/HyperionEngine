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
    HashMap<SamplerDesc, SamplerRef, RenderAllocator> cache;
    SharedMutex mutex;
};

#pragma endregion SamplerCacheImpl

SamplerCache::SamplerCache()
    : m_impl(MakePimpl<SamplerCacheImpl>())
{
    m_impl->cache.Reserve(16);
}

Sampler* SamplerCache::GetOrCreate(const SamplerDesc& samplerDesc)
{
    TSharedLock lock(m_impl->mutex);

    auto it = m_impl->cache.Find(samplerDesc);

    if (it != m_impl->cache.End())
    {
        return it->second.Get();
    }

    lock.Reset();

    TUniqueLock uniqueLock(m_impl->mutex);

    // Check again in case another thread created the sampler
    it = m_impl->cache.Find(samplerDesc);

    if (it != m_impl->cache.End())
    {
        return it->second.Get();
    }

    SamplerRef sampler = g_renderInterface->MakeSampler(samplerDesc);
    CheckResult(sampler->Create());

    AssertDebug(m_impl->cache[samplerDesc] == nullptr);
    m_impl->cache[samplerDesc] = sampler;

    return sampler.Get();
}

} // namespace Hyperion
