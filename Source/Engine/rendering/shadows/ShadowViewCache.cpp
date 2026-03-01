/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/shadows/ShadowViewCache.hpp>

#include <Core/containers/HashMap.hpp>

#include <Core/threading/SharedMutex.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

namespace Hyperion {

struct ShadowViewCacheKey
{
    View* view;
    Light* light;

    HYP_FORCE_INLINE bool operator==(const ShadowViewCacheKey& other) const
    {
        return view == other.view
            && light == other.light;
    }

    HYP_FORCE_INLINE bool operator!=(const ShadowViewCacheKey& other) const
    {
        return view != other.view
            || light != other.light;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode().Combine(view->Id().GetHashCode())
            .Combine(light->Id().GetHashCode());
    }
};

struct ShadowViewCacheEntry
{
    Array<View*> staticViews;
    Array<View*> dynamicViews;

    uint32 lastFrameUsed;
};

class ShadowViewCacheImpl
{
public:
    ~ShadowViewCacheImpl()
    {
        TUniqueLock lock(mutex);
        for (auto& pair : cache)
        {
            ShadowViewCacheEntry& entry = pair.second;

            for (View* view : entry.staticViews)
            {
                if (view)
                {
                    view->Release();
                }
            }

            for (View* view : entry.dynamicViews)
            {
                if (view)
                {
                    view->Release();
                }
            }
        }
    }

    HashMap<ShadowViewCacheKey, ShadowViewCacheEntry> cache;
    SharedMutex mutex;
};

ShadowViewCache::ShadowViewCache()
    : m_impl(MakePimpl<ShadowViewCacheImpl>())
{
}

ShadowViewCache::~ShadowViewCache() = default;

void ShadowViewCache::GetOrCreateShadowView(
    View* view,
    Light* light,
    uint32 cascadeIndex,
    bool isStatic,
    View*& outView) const
{
    Assert(view != nullptr && light != nullptr);

    outView = nullptr;

    ShadowViewCacheKey key {};
    key.view = view;
    key.light = light;

    TSharedLock<SharedMutex> sharedLock(m_impl->mutex);
    TUniqueLock<SharedMutex> uniqueLock; // not locked yet

    auto it = m_impl->cache.Find(key);

    if (it != m_impl->cache.End())
    {
        ShadowViewCacheEntry& entry = it->second;

        auto& views = isStatic ? entry.staticViews : entry.dynamicViews;

        if (cascadeIndex >= entry.staticViews.Size())
        {
            sharedLock.Reset();
            uniqueLock.Reset(m_impl->mutex);

            if (cascadeIndex >= entry.staticViews.Size())
            {
                entry.staticViews.Resize(cascadeIndex + 1);
                entry.dynamicViews.Resize(cascadeIndex + 1);
            }
        }

        outView = views[cascadeIndex];

        if (outView != nullptr)
        {
            return;
        }

        sharedLock.Reset();
        uniqueLock.Reset(m_impl->mutex);

        it = m_impl->cache.Find(key);
        Assert(it != m_impl->cache.End());

        outView = views[cascadeIndex];

        if (!outView)
        {
            outView = new View();

            views[cascadeIndex] = outView;
        }
    }
    else
    {
        sharedLock.Reset();
        uniqueLock.Reset(m_impl->mutex);

        ShadowViewCacheEntry& entry = m_impl->cache[key];

        if (cascadeIndex >= entry.staticViews.Size())
        {
            entry.staticViews.Resize(cascadeIndex + 1);
            entry.dynamicViews.Resize(cascadeIndex + 1);
        }
        
        auto& views = isStatic ? entry.staticViews : entry.dynamicViews;

        outView = new View();

        views[cascadeIndex] = outView;
    }
}


} // namespace Hyperion
