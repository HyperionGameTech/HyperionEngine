/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ASSET_CACHE_HPP
#define HYPERION_ASSET_CACHE_HPP

#include <core/Handle.hpp>

#include <core/containers/HashMap.hpp>

#include <core/threading/Mutex.hpp>

namespace hyperion {

class AssetCachePoolBase
{
public:
    virtual ~AssetCachePoolBase() = default;

    virtual bool Has(const String& key) const = 0;
};

template <class T>
class AssetCachePool : public AssetCachePoolBase
{
    using RefType = typename AssetLoaderWrapper<T>::CastedType;

public:
    AssetCachePool() = default;
    AssetCachePool(const AssetCachePool& other) = delete;
    AssetCachePool& operator=(const AssetCachePool& other) = delete;
    AssetCachePool(AssetCachePool&& other) = delete;
    AssetCachePool& operator=(AssetCachePool&& other) = delete;
    virtual ~AssetCachePool() = default;

    virtual bool Has(const String& key) const override
    {
        Mutex::Guard guard(m_mutex);

        return m_handles.Find(key) != m_handles.End();
    }

    RefType Get(const String& key) const
    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_handles.Find(key);

        if (it != m_handles.End())
        {
            return it->second;
        }

        return RefType {};
    }

    void Set(const String& key, const RefType& value)
    {
        Mutex::Guard guard(m_mutex);

        m_handles.Set(key, value);
    }

    void Set(const String& key, RefType&& value)
    {
        Mutex::Guard guard(m_mutex);

        m_handles.Set(key, std::move(value));
    }

private:
    HashMap<String, RefType> m_handles;
    mutable Mutex m_mutex;
};

class AssetCache
{
public:
    AssetCache() = default;
    AssetCache(const AssetCache& other) = delete;
    AssetCache& operator=(const AssetCache& other) = delete;
    AssetCache(AssetCache&& other) = delete;
    AssetCache& operator=(AssetCache&& other) = delete;
    ~AssetCache() = default;

    template <class T>
    AssetCachePool<T>* GetPool()
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_pools.Find<T>();

        if (it == m_pools.End())
        {
            it = m_pools.Set<T>(MakeUnique<AssetCachePool<T>>()).first;
        }

        return static_cast<AssetCachePool<T>*>(it->second.Get());
    }

private:
    TypeMap<UniquePtr<AssetCachePoolBase>> m_pools;
    mutable Mutex m_mutex;
};

} // namespace hyperion

#endif