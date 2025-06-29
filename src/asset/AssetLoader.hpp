/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_ASSET_LOADER_HPP
#define HYPERION_ASSET_LOADER_HPP

#include <core/Id.hpp>
#include <core/Handle.hpp>

#include <core/memory/Any.hpp>

#include <core/object/HypData.hpp>
#include <core/object/HypObject.hpp>

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

HYP_API extern void OnPostLoad_Impl(const HypClass* hyp_class, void* object_ptr);

struct LoadedAsset
{
    HypData value;

    LoadedAsset() = default;

    LoadedAsset(HypData&& value)
        : value(std::move(value))
    {
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<LoadedAsset, T> && !is_hypdata_v<T> && !std::is_same_v<T, TResult<LoadedAsset, AssetLoadError>>>>
    LoadedAsset(T&& value)
        : value(std::forward<T>(value))
    {
    }

    LoadedAsset(const LoadedAsset& other) = delete;
    LoadedAsset& operator=(const LoadedAsset& other) = delete;
    LoadedAsset(LoadedAsset&& other) noexcept = default;
    LoadedAsset& operator=(LoadedAsset&& other) noexcept = default;
    virtual ~LoadedAsset() = default;

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
    HYP_NODISCARD HYP_FORCE_INLINE auto&& ExtractAs() const&
    {
        if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
        {
            return value.Get<RC<T>>();
        }
        else if constexpr (IsHypObject<T>::value)
        {
            return value.Get<Handle<T>>();
        }
        else
        {
            return value.Get<T>();
        }
    }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE auto&& ExtractAs() &&
    {
        if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
        {
            return std::move(value).Get<RC<T>>();
        }
        else if constexpr (IsHypObject<T>::value)
        {
            return std::move(value).Get<Handle<T>>();
        }
        else
        {
            return std::move(value).Get<T>();
        }
    }

    HYP_API void OnPostLoad();
};

using AssetLoadResult = TResult<LoadedAsset, AssetLoadError>;

template <class T>
struct Asset final : LoadedAsset
{
    Asset()
    {
    }

    Asset(const Asset& other) = delete;
    Asset& operator=(const Asset& other) = delete;

    Asset(Asset&& other) noexcept
        : LoadedAsset(static_cast<LoadedAsset&&>(other))
    {
    }

    Asset& operator=(Asset&& other) noexcept
    {
        static_cast<LoadedAsset&>(*this) = static_cast<LoadedAsset&&>(other);

        return *this;
    }

    Asset(LoadedAsset&& other) noexcept
        : LoadedAsset(std::move(other))
    {
    }

    Asset& operator=(LoadedAsset&& other) noexcept
    {
        static_cast<LoadedAsset&>(*this) = std::move(other);

        return *this;
    }

    virtual ~Asset() override = default;

    decltype(auto) Result() const&
    {
        return LoadedAsset::template ExtractAs<T>();
    }

    decltype(auto) Result() &&
    {
        return LoadedAsset::template ExtractAs<T>();
    }
};

template <class T>
using TAssetLoadResult = TResult<Asset<T>, AssetLoadError>;

// static_assert(sizeof(Asset) == sizeof(LoadedAsset), "sizeof(Asset<T>) must match the size of its parent class, to enable type punning");

HYP_CLASS(Abstract)
class HYP_API AssetLoaderBase : public HypObject<AssetLoaderBase>
{
    HYP_OBJECT_BODY(AssetLoaderBase);

public:
    virtual ~AssetLoaderBase() = default;

    AssetLoadResult Load(AssetManager& asset_manager, const String& path) const;

protected:
    virtual AssetLoadResult LoadAsset(LoaderState& state) const = 0;

    static FilePath GetRebasedFilepath(const FilePath& base_path, const FilePath& filepath);
    Array<FilePath> GetTryFilepaths(const FilePath& original_filepath) const;
};

template <class T>
struct AssetLoadResultWrapper;

template <class T>
struct AssetLoaderWrapper
{
private:
public:
    static constexpr bool is_handle = std::is_base_of_v<HypObjectBase, T>;

    using CastedType = std::conditional_t<is_handle, Handle<T>, Optional<T&>>;

    AssetLoaderBase& loader;

    HYP_DEPRECATED static inline CastedType ExtractAssetValue(HypData& value)
    {
        if constexpr (is_handle)
        {
            if (Handle<T>* handle_ptr = value.TryGet<Handle<T>>())
            {
                return *handle_ptr;
            }

            return Handle<T>::empty;
        }
        else
        {
            return Optional<T&>(value.TryGet<T>());
        }
    }

    HYP_DEPRECATED AssetLoaderWrapper(AssetLoaderBase& loader)
        : loader(loader)
    {
    }
};

template <class T>
struct AssetLoaderWrapper<RC<T>>
{
public:
    using CastedType = RC<T>;

    AssetLoaderBase& loader;

    HYP_DEPRECATED static inline CastedType ExtractAssetValue(HypData& value)
    {
        return value.Get<RC<T>>();
    }

    HYP_DEPRECATED AssetLoaderWrapper(AssetLoaderBase& loader)
        : loader(loader)
    {
    }
};

} // namespace hyperion

#endif