#pragma once

#include "Asset/AxQueue.hpp"
#include "Asset/AxQueueItem.hpp"
#include "AxImage.hpp"
#include "FxObject.hpp"

#include <Core/FxDataNotifier.hpp>
#include <Core/FxRef.hpp>
#include <Core/FxTSRef.hpp>
#include <Core/FxTypes.hpp>
#include <atomic>
#include <thread>

template <typename T>
concept C_IsAsset = std::is_base_of_v<AxBase, T>;

/**
 * Worker thread that waits and processes individual asset loading.
 */
class AxWorker
{
public:
    AxWorker() = default;

    void Create();

    void SubmitItemToLoad(AxQueueItem&& item)
    {
        // Wait for item to no longer be busy
        while (bIsBusy.test()) {
            bIsBusy.wait(true);
        }

        // Set busy
        bIsBusy.test_and_set();

        Item = std::move(item);
        ItemReady.SignalDataWritten();
    }

    void Update();

    void Kill()
    {
        bRunning.store(false);
        // The worker waits on the ItemReady notifier, so we kill the notifier
        ItemReady.Kill();
    }


public:
    AxQueueItem Item;
    AxLoaderBase::Status LoadStatus = AxLoaderBase::Status::eNone;

    FxDataNotifier ItemReady;

    std::atomic_bool bRunning = true;

    std::atomic_flag bIsBusy = ATOMIC_FLAG_INIT;
    std::atomic_flag bDataPendingUpload = ATOMIC_FLAG_INIT;

    std::thread Thread;
};

struct FxLoadObjectOptions
{
    bool bKeepInMemory : 1 = true;
    bool bGeneratePhysicsMesh : 1 = false;
};

class AxManager
{
public:
    AxManager() = default;

    void Start(int32 min_threads);
    void Shutdown();

    void WorkerUpdate();

    static AxManager* GetInstance();

    template <typename T>
        requires C_IsAsset<T>
    FxTSRef<T> NewAsset()
    {
        return FxTSRef<T>::New();
    }


    ////////////////////////////////////////////////
    // Methods to create and load objects
    ////////////////////////////////////////////////


    /**
     * @brief Creates a new `FxObject` and loads the provided asset into it from
     * the path provided.
     */
    FxTSRef<FxObject> LoadObject(const std::string& name, const std::string& path, FxLoadObjectOptions options = {})
    {
        FxTSRef<FxObject> asset = FxTSRef<FxObject>::New();
        LoadObject(name, asset, path, options);

        return asset;
    }

    /**
     * @brief Creates a new `FxObject` and loads the asset into it from
     * the data provided.
     */
    FxTSRef<FxObject> LoadObjectFromMemory(const std::string& name, const uint8* data, uint32 data_size)
    {
        FxTSRef<FxObject> asset = FxTSRef<FxObject>::New();
        LoadObjectFromMemory(name, asset, data, data_size);

        return asset;
    }

    FxTSRef<AxImage> LoadImage(RxImageType image_type, RxImageFormat format, const std::string& path)
    {
        FxTSRef<AxImage> asset = FxTSRef<AxImage>::New();
        LoadImage(image_type, format, asset, path);

        return asset;
    }

    inline FxTSRef<AxImage> LoadImage(const std::string& path, RxImageFormat format)
    {
        return LoadImage(RxImageType::e2d, format, path);
    }

    /**
     * @brief Creates a new `FxObject` and loads the asset into it from
     * the data provided.
     */
    FxTSRef<AxImage> LoadImageFromMemory(RxImageType image_type, RxImageFormat format, const uint8* data,
                                         uint32 data_size)
    {
        FxTSRef<AxImage> asset = FxTSRef<AxImage>::New();
        LoadImageFromMemory(image_type, format, asset, data, data_size);

        return asset;
    }

    inline FxTSRef<AxImage> LoadImageFromMemory(RxImageFormat format, const uint8* data, uint32 data_size)
    {
        return LoadImageFromMemory(RxImageType::e2d, format, data, data_size);
    }


    ////////////////////////////////////////////////
    // Methods to load into existing containers
    ////////////////////////////////////////////////

    void LoadImageFromMemory(RxImageType image_type, RxImageFormat format, FxTSRef<AxImage>& asset, const uint8* data,
                             uint32 data_size);

    /**
     * @brief Loads an asset into the provided asset from the provided data.
     */
    void LoadObjectFromMemory(const std::string& name, FxTSRef<FxObject>& asset, const uint8* data, uint32 data_size);


    /**
     * @brief Loads an object into the provided asset from a path.
     */
    void LoadObject(const std::string& name, FxTSRef<FxObject>& asset, const std::string& path,
                    FxLoadObjectOptions options = {});


    void LoadImage(RxImageType image_type, RxImageFormat format, FxTSRef<AxImage>& asset, const std::string& path);

    /**
     * @brief Loads an Image2D from the path provided into `asset`.
     */
    inline void LoadImage(FxTSRef<AxImage>& asset, RxImageFormat format, const std::string& path)
    {
        return LoadImage(RxImageType::e2d, format, asset, path);
    }
    /**
     * @brief Loads an Image2D from the data provided into `asset`.
     */
    void LoadImageFromMemory(FxTSRef<AxImage>& asset, RxImageFormat format, const uint8* data, uint32 data_size)
    {
        LoadImageFromMemory(RxImageType::e2d, format, asset, data, data_size);
    }

    ~AxManager() { Shutdown(); }


private:
    AxWorker* FindWorkerThread();

    void CheckForUploadableData();
    void CheckForItemsToLoad();

    bool CheckWorkersBusy();

    void AddWorkerThread();
    void AssetManagerUpdate();

    template <typename TAssetType, typename TLoaderType, AxType TEnumValue>
        requires C_IsAsset<TAssetType>
    static void SubmitAssetToLoad(const FxTSRef<TAssetType>& asset, FxTSRef<TLoaderType>& loader,
                                  const std::string& path, const uint8* data = nullptr, uint32 data_size = 0)
    {
        FxAssertMsg(asset->bIsUploadedToGpu == false, "Asset is already uploaded!");

        // FxRef<LoaderType> loader = FxRef<LoaderType>::New();

        AxManager* mgr = GetInstance();

        if (data != nullptr) {
            AxQueueItem queue_item((loader), asset, TEnumValue, data, data_size);
            mgr->mLoadQueue.Push(std::move(queue_item));
        }
        else {
            AxQueueItem queue_item((loader), asset, TEnumValue, path);
            mgr->mLoadQueue.Push(std::move(queue_item));
        }

        mgr->ItemsEnqueued.test_and_set();
        mgr->ItemsEnqueuedNotifier.SignalDataWritten();
    }

public:
    //    FxDataNotifier DataLoaded;
private:
    AxQueue mLoadQueue;

    std::atomic_flag mbActive;

    FxDataNotifier ItemsEnqueuedNotifier;
    std::atomic_flag ItemsEnqueued;

    uint32 mMinThreads = 2;
    // FxSizedArray<std::thread *> mWorkerThreads;
    FxSizedArray<AxWorker> mWorkerThreads;
    std::thread* mpAssetManagerThread;
};
