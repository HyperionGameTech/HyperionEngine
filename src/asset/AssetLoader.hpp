/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_ASSET_LOADER_HPP
#define HYPERION_ASSET_LOADER_HPP

#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <core/memory/Any.hpp>

#include <core/object/HypData.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/Optional.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/serialization/SerializationWrapper.hpp>

#include <core/debug/Debug.hpp>

#include <Constants.hpp>

#include <asset/Loader.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetManager;

using HypData = HypData;

template <class T>
struct AssetLoaderWrapper;

template <class T>
struct Asset;

struct LoadedAsset
{
    HypData                     value;

    Proc<void(LoadedAsset *)>   OnPostLoadProc;

    LoadedAsset() = default;

    LoadedAsset(HypData &&value)
        : value(std::move(value))
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v<LoadedAsset, T> && !std::is_same_v<T, HypData> && !std::is_same_v< T, TResult<LoadedAsset, AssetLoadError> > > >
    LoadedAsset(T &&value)
        : value(std::forward<T>(value))
    {
    }

    LoadedAsset(const LoadedAsset &other)                   = delete;
    LoadedAsset &operator=(const LoadedAsset &other)        = delete;
    LoadedAsset(LoadedAsset &&other) noexcept               = default;
    LoadedAsset &operator=(LoadedAsset &&other) noexcept    = default;
    virtual ~LoadedAsset()                                  = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return value.IsValid();
    }

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

using AssetLoadResult = TResult<LoadedAsset, AssetLoadError>;

template <class T>
struct Asset final : LoadedAsset
{
    using Type = typename SerializationWrapper<T>::Type;

    Asset()
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
        AssertThrowMsg(value.Is<typename SerializationWrapper<Type>::Type>(), "Expected value of type %s", TypeNameWithoutNamespace<Type>().Data());

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

template <class T>
using TAssetLoadResult = TResult<Asset<T>, AssetLoadError>;

// static_assert(sizeof(Asset) == sizeof(LoadedAsset), "sizeof(Asset<T>) must match the size of its parent class, to enable type punning");

class AssetLoaderBase
{
public:
    virtual ~AssetLoaderBase() = default;

    virtual AssetLoadResult Load(AssetManager &asset_manager, const String &path) const = 0;
};

template <class T>
struct AssetLoadResultWrapper;

template <class T>
struct AssetLoaderWrapper
{
private:

public:
    static constexpr bool is_handle = has_handle_definition<T>;

    using CastedType = std::conditional_t<is_handle, Handle<T>, Optional<T &>>;

    AssetLoaderBase &loader;

    HYP_DEPRECATED static inline CastedType ExtractAssetValue(HypData &value)
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

    HYP_DEPRECATED static inline CastedType ExtractAssetValue(HypData &value)
    {   
        return value.Get<RC<T>>();
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

    HYP_API virtual AssetLoadResult Load(AssetManager &asset_manager, const String &path) const override final;

protected:
    virtual AssetLoadResult LoadAsset(LoaderState &state) const = 0;

    static FilePath GetRebasedFilepath(const FilePath &base_path, const FilePath &filepath);
};

} // namespace hyperion

#endif