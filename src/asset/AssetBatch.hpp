#ifndef HYPERION_V2_ASSET_BATCH_HPP
#define HYPERION_V2_ASSET_BATCH_HPP

#include <asset/AssetLoader.hpp>
#include <core/Containers.hpp>
#include <core/Handle.hpp>
#include <scene/Node.hpp>
#include <math/MathUtil.hpp>
#include <TaskSystem.hpp>

#include <type_traits>

namespace hyperion::v2 {

class AssetManager;

struct EnqueuedAsset
{
    String path;
    AssetValue value;

    /*! \brief Retrieve the value of the loaded asset. If the requested type does not match
        the type of the value, if any, held within the variant, no error should occur and an 
        empty container will be returned. */
    template <class T>
    auto Get() -> typename AssetLoaderWrapper<T>::CastedType
    {
        using Wrapper = AssetLoaderWrapper<T>;

        if constexpr (std::is_same_v<typename Wrapper::CastedType, typename Wrapper::ResultType>) {
            if (value.Is<typename Wrapper::ResultType>()) {
                return value.Get<typename Wrapper::ResultType>();
            }
        } else {
            if (value.Is<typename Wrapper::ResultType>()) {
                return value.Get<typename Wrapper::ResultType>().template Cast<T>();
            }
        }

        return typename Wrapper::CastedType();
    }

    explicit operator bool() const { return value.IsValid(); }
};

using AssetMap = FlatMap<String, EnqueuedAsset>;

struct AssetBatch : private TaskBatch
{
private:
    AssetMap enqueued_assets;

    template <class T>
    struct LoadObjectWrapper
    {
        template <class AssetManager, class AssetBatch>
        HYP_FORCE_INLINE void operator()(
            AssetManager &asset_manager,
            AssetBatch &batch,
            const String &key
        )
        {
            std::cout << "Begin load " << key.Data() << std::endl;
            auto &asset = batch.enqueued_assets[key];

            asset.value.Set(static_cast<typename AssetLoaderWrapper<T>::ResultType>(
                asset_manager.template Load<T>(asset.path)
            ));
            std::cout << "End load " << key.Data() << std::endl;
        }
    };

public:
    using TaskBatch::IsCompleted;

    AssetBatch(AssetManager &asset_manager)
        : TaskBatch(),
          asset_manager(asset_manager)
    {
    }

    AssetBatch(const AssetBatch &other) = delete;
    AssetBatch &operator=(const AssetBatch &other) = delete;
    AssetBatch(AssetBatch &&other) noexcept = delete;
    AssetBatch &operator=(AssetBatch &&other) noexcept = delete;
    ~AssetBatch() = default;

    /*! \brief Enqueue an asset of type T to be loaded in this batch.
        Only call this method before LoadAsync() is called. */
    template <class T>
    void Add(const String &key, const String &path)
    {
        AssertThrowMsg(IsCompleted(),
            "Cannot add assets while loading!");

        const auto result = enqueued_assets.Set(key, EnqueuedAsset {
            .path = path
        });

        struct Functor
        {
            AssetBatch *batch;
            String key;

            void operator()()
            {
                DebugLog(LogType::Info, "Begin loading key \"%s\"\n", key.Data());
                LoadObjectWrapper<T>()(batch->asset_manager, *batch, key);
            }
        };

        procs.PushBack(Functor {
            .batch = this,
            .key = key
        });
    }

    /*! \brief Begin loading this batch asynchronously. Note that
        you may not add any more tasks to be loaded once you call this method. */
    void LoadAsync(UInt num_batches = MathUtil::MaxSafeValue<UInt>());
    AssetMap AwaitResults();
    AssetMap ForceLoad();

    AssetManager &asset_manager;

private:
    DynArray<Proc<void>> procs;
};

} // namespace hyperion::V2

#endif