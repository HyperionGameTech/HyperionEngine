/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetPath.hpp>
#include <asset/Loader.hpp>

#include <Core/reflection/ObjId.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/utilities/Optional.hpp>
#include <Core/utilities/FormatFwd.hpp>

#include <Core/logging/LoggerFwd.hpp>

#include <Core/serialization/SerializationWrapper.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/Constants.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetManager;

template <class T>
struct TLoadedAsset;

HYP_API extern void OnPostLoad_Impl(const Class* cls, void* objectPtr);

struct LoadedAsset
{
    BoxedValue value;

    LoadedAsset() = default;

    LoadedAsset(BoxedValue&& value)
        : value(std::move(value))
    {
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<LoadedAsset, T> && !IsBoxedValueV<T> && !std::is_same_v<T, TResult<LoadedAsset, AssetLoadError>>>>
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
        else if constexpr (std::is_base_of_v<ObjectBase, T>)
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
        else if constexpr (std::is_base_of_v<ObjectBase, T>)
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
struct TLoadedAsset final : LoadedAsset
{
    TLoadedAsset()
    {
    }

    TLoadedAsset(const TLoadedAsset& other) = delete;
    TLoadedAsset& operator=(const TLoadedAsset& other) = delete;

    TLoadedAsset(TLoadedAsset&& other) noexcept
        : LoadedAsset(static_cast<LoadedAsset&&>(other))
    {
    }

    TLoadedAsset& operator=(TLoadedAsset&& other) noexcept
    {
        static_cast<LoadedAsset&>(*this) = static_cast<LoadedAsset&&>(other);

        return *this;
    }

    TLoadedAsset(LoadedAsset&& other) noexcept
        : LoadedAsset(std::move(other))
    {
    }

    TLoadedAsset& operator=(LoadedAsset&& other) noexcept
    {
        static_cast<LoadedAsset&>(*this) = std::move(other);

        return *this;
    }

    virtual ~TLoadedAsset() override = default;

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
using TAssetLoadResult = TResult<TLoadedAsset<T>, AssetLoadError>;

HYP_CLASS(Abstract)
class HYP_API AssetLoaderBase : public ObjectBase
{
    HYP_OBJECT_BODY(AssetLoaderBase);

public:
    virtual ~AssetLoaderBase() = default;

    AssetLoadResult Load(AssetManager& assetManager, const String& path, const String& batchIdentifier = String::empty) const;

protected:
    virtual AssetLoadResult LoadAsset(LoaderState& state) const = 0;

    static FilePath GetRebasedFilepath(const FilePath& basePath, const FilePath& filepath);
    Array<FilePath> GetTryFilepaths(const FilePath& originalFilepath) const;
};

template <class T>
struct AssetLoadResultWrapper;

} // namespace Hyperion
