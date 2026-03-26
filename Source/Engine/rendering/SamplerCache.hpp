/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/memory/Pimpl.hpp>

#include <Core/HashCode.hpp>

#include <rendering/RenderObject.hpp>

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