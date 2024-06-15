/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_ASSET_BATCH_HPP
#define HYPERION_ASSET_BATCH_HPP

#include <asset/AssetLoader.hpp>
#include <core/Core.hpp>
#include <core/ID.hpp>

#include <core/containers/String.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/functional/Delegate.hpp>

#include <scene/Node.hpp>

#include <math/MathUtil.hpp>

#include <type_traits>

namespace hyperion {

class AssetManager;

struct EnqueuedAsset
{
    String      path;
    LoadedAsset asset;

    // /*! \brief Retrieve the value of the loaded asset. If the requested type does not match
    //     the type of the value, if any, held within the variant, no error should occur and an 
    //     empty container will be returned. */
    // template <class T>
    // HYP_FORCE_INLINE
    // auto ExtractAs() -> typename AssetLoaderWrapper<T>::CastedType
    //     { return AssetLoaderWrapper<T>::ExtractAssetValue(value); }

    // /*! \brief Alias for ExtractAs<T> */
    // template <class T>
    // HYP_FORCE_INLINE
    // auto Get() -> typename AssetLoaderWrapper<T>::CastedType
    //     { return ExtractAs<T>(); }
    
    // HYP_FORCE_INLINE
    // explicit operator bool() const
    //     { return result.status == LoaderResult::Status::OK && value.HasValue(); }

    explicit operator bool() const
        { return bool(asset); }
};

using AssetMap = FlatMap<String, LoadedAsset>;


struct AssetBatchCallbackData
{
    using AssetKeyValuePair = Pair<const String &, LoadedAsset &>;
    using DataType = Variant<AssetKeyValuePair, std::reference_wrapper<AssetMap>>;

    DataType data;

    AssetBatchCallbackData() = default;

    AssetBatchCallbackData(const String &asset_key, LoadedAsset &asset)
        : data { AssetKeyValuePair { asset_key, asset } }
    {
    }

    AssetBatchCallbackData(const AssetBatchCallbackData &)                  = default;
    AssetBatchCallbackData &operator=(const AssetBatchCallbackData &)       = default;
    AssetBatchCallbackData(AssetBatchCallbackData &&) noexcept              = default;
    AssetBatchCallbackData &operator=(AssetBatchCallbackData &&) noexcept   = default;
    ~AssetBatchCallbackData()                                               = default;

    const String &GetAssetKey()
        { return data.Get<AssetKeyValuePair>().first; }

    const LoadedAsset &GetAsset()
        { return data.Get<AssetKeyValuePair>().second; }
};

struct AssetBatchCallbacks
{
    Delegate< void, const AssetBatchCallbackData & >    OnItemComplete;
    Delegate< void, const AssetBatchCallbackData & >    OnItemFailed;
};

template <class T>
struct LoadObjectWrapper
{
    template <class AssetManager>
    HYP_FORCE_INLINE
    void Process(
        AssetManager *asset_manager,
        AssetMap *map,
        const String &key,
        const String &path,
        AssetBatchCallbacks *callbacks
    )
    {
        LoadedAsset &asset = (*map)[key];
        asset = asset_manager->template Load<T>(path);

        // EnqueuedAsset &asset = (*map)[key];
        // asset.value = AssetLoaderWrapper<T>::MakeResultType(asset_manager->template Load<T>(asset.path, asset.result));

        if (asset.IsOK()) {
            if (callbacks) {
                callbacks->OnItemComplete(AssetBatchCallbackData(key, asset));
            }
        } else {
            if (callbacks) {
                callbacks->OnItemFailed(AssetBatchCallbackData(key, asset));
            }

            asset.value.Reset();
        }
    }
};

struct ProcessAssetFunctorBase
{
    virtual ~ProcessAssetFunctorBase() = default;

    virtual void operator()(AssetManager *asset_manager, AssetMap *asset_map) = 0;
};

template <class T>
struct ProcessAssetFunctor : public ProcessAssetFunctorBase
{
    String              key;
    String              path;
    AssetBatchCallbacks *callbacks;

    ProcessAssetFunctor(const String &key, const String &path, AssetBatchCallbacks *callbacks)
        : key(key),
          path(path),
          callbacks(callbacks)
    {
    }

    ProcessAssetFunctor(const ProcessAssetFunctor &)                = delete;
    ProcessAssetFunctor &operator=(const ProcessAssetFunctor &)     = delete;
    ProcessAssetFunctor(ProcessAssetFunctor &&) noexcept            = delete;
    ProcessAssetFunctor &operator=(ProcessAssetFunctor &&) noexcept = delete;

    virtual ~ProcessAssetFunctor() override = default;

    virtual void operator()(AssetManager *asset_manager, AssetMap *asset_map) override
    {
        LoadObjectWrapper<T>{}.Process(asset_manager, asset_map, key, path, callbacks);
    }
};

struct AssetBatch : private TaskBatch
{
private:
    // make it be a ptr so this can be moved
    UniquePtr<AssetMap> enqueued_assets;

public:
    using TaskBatch::IsCompleted;

    AssetBatch(AssetManager *asset_manager)
        : TaskBatch(),
          enqueued_assets(new AssetMap),
          asset_manager(asset_manager)
    {
        AssertThrow(asset_manager != nullptr);
    }

    AssetBatch(const AssetBatch &other)                 = delete;
    AssetBatch &operator=(const AssetBatch &other)      = delete;
    AssetBatch(AssetBatch &&other) noexcept             = delete;
    AssetBatch &operator=(AssetBatch &&other) noexcept  = delete;

    ~AssetBatch()
    {
        if (enqueued_assets != nullptr) {
            // all tasks must be completed or the destruction of enqueued_assets
            // will cause a dangling ptr issue.
            AssertThrowMsg(
                enqueued_assets->Empty(),
                "All enqueued assets must be completed before the destructor of AssetBatch is called or else dangling pointer issues will occur."
            );
        }
    }

    AssetBatchCallbacks &GetCallbacks()
        { return callbacks; }

    const AssetBatchCallbacks &GetCallbacks() const
        { return callbacks; }

    /*! \brief Enqueue an asset of type T to be loaded in this batch.
        Only call this method before LoadAsync() is called. */
    HYP_API void Add(const String &key, const String &path);

    /*! \brief Begin loading this batch asynchronously. Note that
        you may not add any more tasks to be loaded once you call this method. */
    HYP_API void LoadAsync(uint num_batches = MathUtil::MaxSafeValue<uint>());

    [[nodiscard]]
    HYP_API AssetMap AwaitResults();

    [[nodiscard]]
    HYP_API AssetMap ForceLoad();

    AssetManager                                *asset_manager;

    /*! \brief Functions bound to this delegates are called in
     *  the game thread. */
    Delegate<void, AssetMap &>                  OnComplete;

private:
    Array<UniquePtr<ProcessAssetFunctorBase>>   procs;
    AssetBatchCallbacks                         callbacks;
};

} // namespace hyperion

#endif