#ifndef HYPERION_V2_ASSET_CACHE_HPP
#define HYPERION_V2_ASSET_CACHE_HPP

#include <core/Core.hpp>
#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <core/Handle.hpp>
#include <scene/Node.hpp>
#include <Constants.hpp>

#include "Loader.hpp"

#include <system/Debug.hpp>

#include <mutex>

namespace hyperion::v2 {

class AssetCachePoolBase
{
public:
    virtual ~AssetCachePoolBase() = default;

    virtual bool Has(const String &key) const = 0;
};

template <class T>
class AssetCachePool : public AssetCachePoolBase
{
    using RefType = typename AssetLoaderWrapper<T>::CastedType;

public:
    AssetCachePool() = default;
    AssetCachePool(const AssetCachePool &other) = delete;
    AssetCachePool &operator=(const AssetCachePool &other) = delete;
    AssetCachePool(AssetCachePool &&other) = delete;
    AssetCachePool &operator=(AssetCachePool &&other) = delete;
    virtual ~AssetCachePool() = default;

    virtual bool Has(const String &key) const override
    {
        std::lock_guard guard(m_mutex);

        return m_handles.Find(key) != m_handles.End();
    }

    const RefType &Get(const String &key) const
    {
        std::lock_guard guard(m_mutex);

        const auto it = m_handles.Find(key);

        if (it != m_handles.End()) {
            return it->value;
        }

        return RefType { };
    }

    void Set(const String &key, const RefType &value)
    {
        std::lock_guard guard(m_mutex);

        m_handles.Set(key, value);
    }

    void Set(const String &key, RefType &&value)
    {
        std::lock_guard guard(m_mutex);

        m_handles.Set(key, std::move(value));
    }

private:
    HashMap<String, RefType> m_handles;
    mutable std::mutex m_mutex;
};

class AssetCache
{
public:
    AssetCache() = default;
    AssetCache(const AssetCache &other) = delete;
    AssetCache &operator=(const AssetCache &other) = delete;
    AssetCache(AssetCache &&other) = delete;
    AssetCache &operator=(AssetCache &&other) = delete;
    ~AssetCache() = default;

    template <class T>
    AssetCachePool<T> *GetPool()
    {
        std::lock_guard guard(m_mutex);

        auto it = m_pools.Find<T>();

        if (it == m_pools.End()) {
            it = m_pools.Set<T>(UniquePtr<AssetCachePool<T>>::Construct()).first;
        }

        return static_cast<AssetCachePool<T> *>(it->second.Get());
    }

private:
    TypeMap<UniquePtr<AssetCachePoolBase>> m_pools;
    mutable std::mutex m_mutex;
};

} // namespace hyperion::v2

#endif