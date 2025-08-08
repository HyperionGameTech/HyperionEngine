/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <core/containers/String.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/functional/Delegate.hpp>

#include <scene/Node.hpp>

#include <core/math/MathUtil.hpp>

#include <type_traits>

namespace hyperion {

class AssetManager;

using AssetMap = FlatMap<String, LoadedAsset>;

struct AssetBatchCallbackData
{
    using AssetKeyValuePair = Pair<String, LoadedAsset*>;

    AssetKeyValuePair data;

    AssetBatchCallbackData(const String& assetKey, LoadedAsset* asset)
        : data { assetKey, asset }
    {
    }

    AssetBatchCallbackData(const AssetBatchCallbackData&) = default;
    AssetBatchCallbackData& operator=(const AssetBatchCallbackData&) = default;
    AssetBatchCallbackData(AssetBatchCallbackData&&) noexcept = default;
    AssetBatchCallbackData& operator=(AssetBatchCallbackData&&) noexcept = default;
    ~AssetBatchCallbackData() = default;

    HYP_FORCE_INLINE const String& GetAssetKey()
    {
        return data.first;
    }

    HYP_FORCE_INLINE LoadedAsset* GetAsset()
    {
        return data.second;
    }
};

struct AssetBatchCallbacks
{
    Delegate<void, const AssetBatchCallbackData&> OnItemComplete;
    Delegate<void, const AssetBatchCallbackData&> OnItemFailed;
};

template <class T>
struct LoadObjectWrapper
{
    template <class AssetManager>
    HYP_FORCE_INLINE void Process(
        AssetManager* assetManager,
        AssetMap* map,
        const String& key,
        const String& path,
        AssetBatchCallbacks* callbacks)
    {
        auto it = map->Find(key);
        Assert(it != map->End());

        LoadedAsset& asset = it->second;

        if (AssetLoadResult result = assetManager->template Load<T>(path))
        {
            asset = std::move(result.GetValue());
        }

        if (asset.IsValid())
        {
            if (callbacks)
            {
                callbacks->OnItemComplete(AssetBatchCallbackData(key, &asset));
            }
        }
        else
        {
            if (callbacks)
            {
                callbacks->OnItemFailed(AssetBatchCallbackData(key, &asset));
            }

            asset.value.Reset();
        }
    }
};

struct ProcessAssetFunctorBase
{
    virtual ~ProcessAssetFunctorBase() = default;

    virtual TResult<void, AssetLoadError> operator()(AssetManager& assetManager, AssetMap& assetMap) = 0;
};

template <class T>
struct ProcessAssetFunctor final : public ProcessAssetFunctorBase
{
    String batchIdentifier;
    String key;
    String path;
    AssetBatchCallbacks* callbacks;

    ProcessAssetFunctor(const String& batchIdentifier, const String& key, const String& path, AssetBatchCallbacks* callbacks)
        : batchIdentifier(batchIdentifier),
          key(key),
          path(path),
          callbacks(callbacks)
    {
    }

    ProcessAssetFunctor(const ProcessAssetFunctor&) = delete;
    ProcessAssetFunctor& operator=(const ProcessAssetFunctor&) = delete;
    ProcessAssetFunctor(ProcessAssetFunctor&&) noexcept = delete;
    ProcessAssetFunctor& operator=(ProcessAssetFunctor&&) noexcept = delete;

    virtual ~ProcessAssetFunctor() override = default;

    virtual TResult<void, AssetLoadError> operator()(AssetManager& assetManager, AssetMap& assetMap) override
    {
        return Process_Internal(assetManager, assetMap);
    }

private:
    template <class AssetManagerType>
    TResult<void, AssetLoadError> Process_Internal(AssetManagerType& assetManager, AssetMap& assetMap)
    {
        auto it = assetMap.Find(key);
        Assert(it != assetMap.End());

        LoadedAsset& asset = it->second;

        if (AssetLoadResult result = assetManager.template Load<T>(path, /* batchIdentifier */ batchIdentifier))
        {
            asset = std::move(result.GetValue());
        }
        else
        {
            return result.GetError();
        }

        if (asset.IsValid())
        {
            asset.OnPostLoad();

            if (callbacks)
            {
                callbacks->OnItemComplete(AssetBatchCallbackData(key, &asset));
            }
        }
        else
        {
            if (callbacks)
            {
                callbacks->OnItemFailed(AssetBatchCallbackData(key, &asset));
            }

            asset.value.Reset();
        }

        return {};
    }
};

class AssetBatch : public EnableRefCountedPtrFromThis<AssetBatch>, private TaskBatch
{
private:
    // make it be a ptr so this can be moved
    UniquePtr<AssetMap> m_assetMap;

public:
    using TaskBatch::IsCompleted;

    HYP_API AssetBatch(const Handle<AssetManager>& assetManager);
    HYP_API AssetBatch(const Handle<AssetManager>& assetManager, const String& identifier);

    AssetBatch(const AssetBatch& other) = delete;
    AssetBatch& operator=(const AssetBatch& other) = delete;
    AssetBatch(AssetBatch&& other) noexcept = delete;
    AssetBatch& operator=(AssetBatch&& other) noexcept = delete;

    virtual ~AssetBatch() override
    {
        if (m_assetMap != nullptr)
        {
            // all tasks must be completed or the destruction of enqueuedAssets
            // will cause a dangling ptr issue.
            Assert(
                m_assetMap->Empty(),
                "All enqueued assets must be completed before the destructor of AssetBatch is called or else dangling pointer issues will occur.");
        }
    }

    HYP_FORCE_INLINE AssetBatchCallbacks& GetCallbacks()
    {
        return m_callbacks;
    }

    HYP_FORCE_INLINE const AssetBatchCallbacks& GetCallbacks() const
    {
        return m_callbacks;
    }

    /*! \brief Enqueue an asset of type T to be loaded in this batch.
        Only call this method before LoadAsync() is called. */
    HYP_API void Add(const String& key, const String& path);
    HYP_API void Add(const String& key, const String& path, const Proc<void(LoadedAsset&)>& callback);

    /*! \brief Begin loading this batch asynchronously. Note that
        you may not add any more tasks to be loaded once you call this method. */
    HYP_API void LoadAsync(uint32 numBatches = MathUtil::MaxSafeValue<uint32>());

    HYP_API AssetMap AwaitResults();
    HYP_API AssetMap ForceLoad();

    /*! \brief Functions bound to this delegates are called in
     *  the game thread. */
    Delegate<void, AssetMap&> OnComplete;

private:
    String m_identifier;
    
    Handle<AssetManager> m_assetManager;
    
    Array<UniquePtr<ProcessAssetFunctorBase>> m_procs;
    AssetBatchCallbacks m_callbacks;

    Array<Array<TResult<void, AssetLoadError>>, DynamicAllocator> m_results;
};

} // namespace hyperion
