/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_ASSET_LOADER_HPP
#define HYPERION_ASSET_LOADER_HPP

#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <core/memory/Any.hpp>

#include <core/object/HypData.hpp>

#include <core/utilities/Optional.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/serialization/SerializationWrapper.hpp>

#include <core/debug/Debug.hpp>

#include <scene/Node.hpp>

#include <Constants.hpp>

#include <asset/Loader.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetManager;

using AssetValue = HypData;

template <class T>
struct AssetLoaderWrapper;

template <class T>
struct Asset;

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

    template <class T, typename = std::enable_if_t< !std::is_same_v<T, AssetValue> > >
    LoadedAsset(LoaderResult result, T &&value)
        : result(std::move(result)),
          value(std::forward<T>(value))
    {
    }

    LoadedAsset(const LoadedAsset &other)                   = delete;
    LoadedAsset &operator=(const LoadedAsset &other)        = delete;
    LoadedAsset(LoadedAsset &&other) noexcept               = default;
    LoadedAsset &operator=(LoadedAsset &&other) noexcept    = default;
    virtual ~LoadedAsset()                                  = default;

    HYP_FORCE_INLINE operator bool() const
        { return IsOK(); }

    HYP_FORCE_INLINE bool IsOK() const
        { return result.status == LoaderResult::Status::OK; }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE auto &ExtractAs()
    {
        return static_cast<Asset<T> *>(this)->Result();
    }

    HYP_FORCE_INLINE void OnPostLoad()
    {
        if (!OnPostLoadProc.IsValid()) {
            return;
        }

        OnPostLoadProc(this);
    }
};

template <class T>
struct Asset final : LoadedAsset
{
    using Type = typename SerializationWrapper<T>::Type;

    Asset()
    {
        InitCallbacks();
    }

    Asset(LoaderResult result)
        : LoadedAsset(std::move(result))
    {
        InitCallbacks();
    }

    Asset(LoaderResult result, AssetValue &&value)
        : LoadedAsset(std::move(result), std::move(value))
    {
        InitCallbacks();
    }

    Asset(const Asset &other)                   = delete;
    Asset &operator=(const Asset &other)        = delete;

    Asset(Asset &&other) noexcept
        : LoadedAsset(static_cast<LoadedAsset &&>(other))
    {
    }

    Asset &operator=(Asset &&other) noexcept
    {
        static_cast<LoadedAsset &>(*this) = static_cast<LoadedAsset &&>(other);

        return *this;
    }

    Asset(LoadedAsset &&other) noexcept
        : LoadedAsset(std::move(other))
    {
        InitCallbacks();
    }
    
    Asset &operator=(LoadedAsset &&other) noexcept
    {
        static_cast<LoadedAsset &>(*this) = std::move(other);

        InitCallbacks();

        return *this;
    }

    virtual ~Asset() override                   = default;

    HYP_FORCE_INLINE auto &Result()
    {
        AssertThrowMsg(IsOK() && value.Is<typename SerializationWrapper<Type>::Type>(), "Asset did not load successfully");

        return value.Get<typename SerializationWrapper<Type>::Type>();
    }

private:
    void InitCallbacks()
    {
        OnPostLoadProc = [](LoadedAsset *asset)
        {
            SerializationWrapper<T>::OnPostLoad(static_cast<Asset *>(asset)->Result());
        };
    }
};

// static_assert(sizeof(Asset) == sizeof(LoadedAsset), "sizeof(Asset<T>) must match the size of its parent class, to enable type punning");

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

    using CastedType = std::conditional_t<is_handle, Handle<T>, Optional<T &>>;

    AssetLoaderBase &loader;

    HYP_DEPRECATED static inline CastedType ExtractAssetValue(AssetValue &value)
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

    HYP_DEPRECATED AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }
};

template <class T>
struct AssetLoaderWrapper<RC<T>>
{
public:
    using CastedType = RC<T>;

    AssetLoaderBase &loader;

    HYP_DEPRECATED static inline CastedType ExtractAssetValue(AssetValue &value)
    {   
        return value.Get<RC<T>>();
    }

    HYP_DEPRECATED AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }
};

template <>
struct AssetLoaderWrapper<Node>
{
    using CastedType = NodeProxy;

    AssetLoaderBase &loader;

    HYP_DEPRECATED static inline CastedType ExtractAssetValue(AssetValue &value)
    {
        auto result = value.TryGet<NodeProxy>();

        if (!result) {
            return CastedType { };
        }

        if (*result) {
            (*result)->SetScene(nullptr); 
        }

        return *result;
    }

    HYP_DEPRECATED AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }
};

// AssetLoader
class AssetLoader : public AssetLoaderBase
{
protected:
    Array<FilePath> GetTryFilepaths(const FilePath &original_filepath) const;

public:
    virtual ~AssetLoader() override = default;

    HYP_API virtual LoadedAsset Load(AssetManager &asset_manager, const String &path) const override final;

protected:
    virtual LoadedAsset LoadAsset(LoaderState &state) const = 0;

    static FilePath GetRebasedFilepath(const FilePath &base_path, const FilePath &filepath);
};

} // namespace hyperion

#endif