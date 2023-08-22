#ifndef HYPERION_V2_ASSET_BATCH_HPP
#define HYPERION_V2_ASSET_BATCH_HPP

#include <asset/AssetLoader.hpp>
#include <core/Core.hpp>
#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <scene/Node.hpp>
#include <math/MathUtil.hpp>
#include <TaskSystem.hpp>

#include <type_traits>

namespace hyperion::v2 {

class AssetManager;

struct EnqueuedAsset
{
    String path;
    LoaderResult result;
    AssetValue value;

    /*! \brief Retrieve the value of the loaded asset. If the requested type does not match
        the type of the value, if any, held within the variant, no error should occur and an 
        empty container will be returned. */
    template <class T>
    HYP_FORCE_INLINE
    auto ExtractAs() -> typename AssetLoaderWrapper<T>::CastedType
        { return AssetLoaderWrapper<T>::ExtractAssetValue(value); }

    /*! \brief Alias for ExtractAs<T> */
    template <class T>
    HYP_FORCE_INLINE
    auto Get() -> typename AssetLoaderWrapper<T>::CastedType
        { return ExtractAs<T>(); }
    
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return result.status == LoaderResult::Status::OK && value.IsValid(); }
};

using AssetMap = FlatMap<String, EnqueuedAsset>;

struct AssetBatch : private TaskBatch
{
private:
    // make it be a ptr so this can be moved
    UniquePtr<AssetMap> enqueued_assets;

    template <class T>
    struct LoadObjectWrapper
    {
        template <class AssetManager>
        HYP_FORCE_INLINE void operator()(
            AssetManager *asset_manager,
            AssetMap *map,
            const String &key
        )
        {
            EnqueuedAsset &asset = (*map)[key];

            asset.value.Set(AssetLoaderWrapper<T>::MakeResultType(
                asset_manager->template Load<T>(asset.path, asset.result)
            ));

            if (asset.result != LoaderResult::Status::OK) {
                asset.value.Reset();
            }
        }
    };

public:
    using TaskBatch::IsCompleted;

    AssetBatch(AssetManager *asset_manager)
        : TaskBatch(),
          enqueued_assets(new AssetMap),
          asset_manager(asset_manager)
    {
        AssertThrow(asset_manager != nullptr);
    }

    AssetBatch(const AssetBatch &other) = delete;
    AssetBatch &operator=(const AssetBatch &other) = delete;
    AssetBatch(AssetBatch &&other) noexcept = delete;
    AssetBatch &operator=(AssetBatch &&other) noexcept = delete;

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

    /*! \brief Enqueue an asset of type T to be loaded in this batch.
        Only call this method before LoadAsync() is called. */
    template <class T>
    void Add(const String &key, const String &path)
    {
        AssertThrowMsg(IsCompleted(),
            "Cannot add assets while loading!");

        AssertThrowMsg(enqueued_assets != nullptr,
            "AssetBatch is in invalid state");

        enqueued_assets->Set(key, EnqueuedAsset {
            .path = path
        });

        struct Functor
        {
            String key;

            void operator()(AssetManager *asset_manager, AssetMap *asset_map)
            {
                LoadObjectWrapper<T>()(asset_manager, asset_map, key);
            }
        };

        procs.PushBack(Functor { key });
    }

    /*! \brief Begin loading this batch asynchronously. Note that
        you may not add any more tasks to be loaded once you call this method. */
    void LoadAsync(UInt num_batches = MathUtil::MaxSafeValue<UInt>());
    [[nodiscard]] AssetMap AwaitResults();
    [[nodiscard]] AssetMap ForceLoad();

    AssetManager *asset_manager;

private:
    Array<Proc<void, AssetManager *, AssetMap *>> procs;
};

} // namespace hyperion::V2

#endif