/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_ASSET_LOADER_HPP
#define HYPERION_ASSET_LOADER_HPP

#include <core/Core.hpp>
#include <core/ID.hpp>
#include <core/Handle.hpp>
#include <core/memory/Any.hpp>
#include <core/utilities/Optional.hpp>
#include <core/logging/LoggerFwd.hpp>
#include <core/system/Debug.hpp>
#include <scene/Node.hpp>
#include <Constants.hpp>

#include <asset/Loader.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetManager;

using AssetValue = Any;

template <class T>
struct AssetLoaderWrapper;

template <class T>
struct LoadedAssetInstance;

struct LoadedAsset
{
    LoaderResult                result;
    AssetValue                  value;

    Proc<void, LoadedAsset *>   OnPostLoadProc;

    LoadedAsset() = default;

    LoadedAsset(LoaderResult result)
        : result(std::move(result))
    {
    }

    LoadedAsset(LoaderResult result, AssetValue &&value)
        : result(std::move(result)),
          value(std::move(value))
    {
    }

    LoadedAsset(const LoadedAsset &other)                   = delete;
    LoadedAsset &operator=(const LoadedAsset &other)        = delete;
    LoadedAsset(LoadedAsset &&other) noexcept               = default;
    LoadedAsset &operator=(LoadedAsset &&other) noexcept    = default;
    ~LoadedAsset()                                          = default;

    HYP_NODISCARD HYP_FORCE_INLINE
    operator bool() const
        { return IsOK(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsOK() const
        { return result.status == LoaderResult::Status::OK; }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    typename AssetLoaderWrapper<T>::CastedType ExtractAs()
    {
        return static_cast<LoadedAssetInstance<T> *>(this)->Result();
    }

    HYP_FORCE_INLINE
    void OnPostLoad()
    {
        if (!OnPostLoadProc.IsValid()) {
            return;
        }

        OnPostLoadProc(this);
    }
};

template <class T>
struct LoadedAssetInstance : LoadedAsset
{
    using CastedType = typename AssetLoaderWrapper<T>::CastedType;

    LoadedAssetInstance()
    {
        InitCallbacks();
    }

    LoadedAssetInstance(LoaderResult result)
        : LoadedAsset(std::move(result))
    {
        InitCallbacks();
    }

    LoadedAssetInstance(LoaderResult result, AssetValue &&value)
        : LoadedAsset(std::move(result), std::move(value))
    {
        InitCallbacks();
    }

    LoadedAssetInstance(const LoadedAssetInstance &other)                   = delete;
    LoadedAssetInstance &operator=(const LoadedAssetInstance &other)        = delete;

    LoadedAssetInstance(LoadedAssetInstance &&other) noexcept
        : LoadedAsset(static_cast<LoadedAsset &&>(other))
    {
    }

    LoadedAssetInstance &operator=(LoadedAssetInstance &&other) noexcept
    {
        static_cast<LoadedAsset &>(*this) = static_cast<LoadedAsset &&>(other);

        return *this;
    }

    LoadedAssetInstance(LoadedAsset &&other) noexcept
        : LoadedAsset(std::move(other))
    {
        InitCallbacks();
    }
    
    LoadedAssetInstance &operator=(LoadedAsset &&other) noexcept
    {
        static_cast<LoadedAsset &>(*this) = std::move(other);

        InitCallbacks();

        return *this;
    }

    CastedType &Result()
    {
        AssertThrowMsg(IsOK() && value.Is<CastedType>(), "Asset did not load successfully");

        return value.Get<CastedType>();
    }

private:
    void InitCallbacks()
    {
        OnPostLoadProc = [](LoadedAsset *asset)
        {
            AssetLoaderWrapper<T>::OnPostLoad(static_cast<LoadedAssetInstance *>(asset)->Result());
        };
    }
};

// static_assert(sizeof(LoadedAssetInstance) == sizeof(LoadedAsset), "sizeof(LoadedAssetInstance<T>) must match the size of its parent class, to enable type punning");

class AssetLoaderBase
{
public:
    virtual ~AssetLoaderBase() = default;

    virtual LoadedAsset Load(AssetManager &asset_manager, const String &path) const = 0;
};

template <class T>
struct AssetLoadResultWrapper;

template <class T>
struct AssetLoaderWrapper
{
private:

public:
    static constexpr bool is_handle = has_opaque_handle_defined<T>;

    using ResultType = Any;
    using CastedType = std::conditional_t<is_handle, Handle<T>, Optional<T &>>;

    AssetLoaderBase &loader;

    HYP_DEPRECATED
    static inline CastedType ExtractAssetValue(AssetValue &value)
    {   
        if constexpr (is_handle) {
            if (Handle<T> *handle_ptr = value.TryGet<Handle<T>>()) {
                return *handle_ptr;
            }

            return Handle<T>::empty;
        } else {
            return Optional<T &>(value.TryGet<T>());
        }
    }

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    LoadedAssetInstance<T> GetLoadedAsset(AssetManager &asset_manager, const UTF8StringView &path)
    {
        return LoadedAssetInstance<T>(loader.Load(asset_manager, path));
    }

    static void OnPostLoad(CastedType &value) { }
};

template <class T>
struct AssetLoaderWrapper<RC<T>>
{
public:
    using ResultType = Any;
    using CastedType = RC<T>;

    AssetLoaderBase &loader;

    HYP_DEPRECATED
    static inline CastedType ExtractAssetValue(AssetValue &value)
    {   
        return value.Get<RC<T>>();
    }

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    LoadedAssetInstance<RC<T>> GetLoadedAsset(AssetManager &asset_manager, const UTF8StringView &path)
    {
        return LoadedAssetInstance<RC<T>>(loader.Load(asset_manager, path));
    }

    static void OnPostLoad(CastedType &value) { }
};

template <>
struct AssetLoaderWrapper<Node>
{
    using ResultType = Any;
    using CastedType = NodeProxy;

    AssetLoaderBase &loader;

    HYP_DEPRECATED
    static inline CastedType ExtractAssetValue(AssetValue &value)
    {
        NodeProxy *result = value.TryGet<NodeProxy>();

        if (!result) {
            return CastedType { };
        }

        if (result->IsValid()) {
            (*result)->SetScene(nullptr); 
        }

        return *result;
    }

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    LoadedAssetInstance<Node> GetLoadedAsset(AssetManager &asset_manager, const UTF8StringView &path)
    {
        LoadedAssetInstance<Node> result(loader.Load(asset_manager, path));

        if (result.IsOK()) {
            NodeProxy &node_proxy = result.Result();

            if (node_proxy.IsValid()) {
                node_proxy->SetScene(nullptr); // sets scene to be the "detached" scene for the current thread we're on.
            }
        }

        return result;
    }

    static void OnPostLoad(CastedType &value)
    {
        value->SetScene(nullptr);
    }
};

// AssetLoader
class AssetLoader : public AssetLoaderBase
{
protected:
    static inline auto GetTryFilepaths(const FilePath &filepath, const FilePath &original_filepath)
    {
        const FilePath current_path = FilePath::Current();

        FixedArray<FilePath, 3> paths {
            FilePath::Relative(original_filepath, current_path),
            FilePath::Relative(filepath, current_path),
            filepath
        };

        return paths;
    }

public:
    virtual ~AssetLoader() override = default;

    HYP_API virtual LoadedAsset Load(AssetManager &asset_manager, const String &path) const override final;

protected:
    virtual LoadedAsset LoadAsset(LoaderState &state) const = 0;

    FilePath GetRebasedFilepath(const AssetManager &asset_manager, const FilePath &filepath) const;
};

} // namespace hyperion

#endif