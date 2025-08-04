/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/ObjId.hpp>
#include <core/Handle.hpp>

#include <core/memory/Any.hpp>

#include <core/object/HypData.hpp>
#include <core/object/HypObject.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/FormatFwd.hpp>

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
struct TLoadedAsset;

HYP_API extern void OnPostLoad_Impl(const HypClass* hypClass, void* objectPtr);

HYP_STRUCT()
struct AssetPath
{
    HYP_FIELD()
    Name* chain = nullptr;

    AssetPath() = default;

    AssetPath(const AssetPath& other)
    {
        if (other.chain)
        {
            uint32 count = 0;

            Name* curr = other.chain;

            while (curr->IsValid())
            {
                ++count;
                ++curr;
            }

            chain = new Name[count + 1];

            for (uint32 i = 0; i < count; i++)
            {
                chain[i] = other.chain[i];
            }

            chain[count] = Name::Invalid();
        }
    }

    AssetPath& operator=(const AssetPath& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if (chain)
        {
            delete[] chain;
        }

        if (other.chain)
        {
            uint32 count = 0;

            Name* curr = other.chain;

            while (curr->IsValid())
            {
                ++count;
                ++curr;
            }

            chain = new Name[count + 1];

            for (uint32 i = 0; i < count; i++)
            {
                chain[i] = other.chain[i];
            }

            chain[count] = Name::Invalid();
        }
        else
        {
            chain = nullptr;
        }

        return *this;
    }

    AssetPath(AssetPath&& other) noexcept
        : chain(other.chain)
    {
        other.chain = nullptr;
    }

    AssetPath& operator=(AssetPath&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (chain)
        {
            delete[] chain;
        }

        chain = other.chain;
        other.chain = nullptr;

        return *this;
    }

    ~AssetPath()
    {
        if (chain)
        {
            delete[] chain;
        }
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsValid() const
    {
        return chain && chain[0].IsValid();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    HYP_METHOD()
    String ToString() const
    {
        if (!chain)
        {
            return String::empty;
        }

        String result;

        Name* curr = chain;

        while (curr->IsValid())
        {
            if (chain != curr)
            {
                result.Append("/");
            }

            result.Append(curr->LookupString());

            ++curr;
        }

        return result;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return ToString().GetHashCode();
    }

    HYP_METHOD(Property = "Chain", Serialize = true)
    Array<Name> GetChain() const
    {
        Array<Name> result;

        if (chain)
        {
            Name* curr = chain;

            while (curr->IsValid())
            {
                result.PushBack(*curr);
                ++curr;
            }
        }

        return result;
    }

    HYP_METHOD(Property = "Chain", Serialize = true)
    void SetChain(const Array<Name>& names)
    {
        if (chain)
        {
            delete[] chain;
            chain = nullptr;
        }

        if (names.Empty())
        {
            return;
        }

        chain = new Name[names.Size() + 1];

        for (SizeType i = 0; i < names.Size(); i++)
        {
            chain[i] = names[i];
        }

        chain[names.Size()] = Name::Invalid();
    }
};

struct LoadedAsset
{
    HypData value;

    LoadedAsset() = default;

    LoadedAsset(HypData&& value)
        : value(std::move(value))
    {
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<LoadedAsset, T> && !isHypData<T> && !std::is_same_v<T, TResult<LoadedAsset, AssetLoadError>>>>
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
class HYP_API AssetLoaderBase : public HypObjectBase
{
    HYP_OBJECT_BODY(AssetLoaderBase);

public:
    virtual ~AssetLoaderBase() = default;

    AssetLoadResult Load(AssetManager& assetManager, const String& path) const;

protected:
    virtual AssetLoadResult LoadAsset(LoaderState& state) const = 0;

    static FilePath GetRebasedFilepath(const FilePath& basePath, const FilePath& filepath);
    Array<FilePath> GetTryFilepaths(const FilePath& originalFilepath) const;
};

template <class T>
struct AssetLoadResultWrapper;

template <class T>
struct AssetLoaderWrapper
{
private:
public:
    static constexpr bool isHandle = std::is_base_of_v<HypObjectBase, T>;

    using CastedType = std::conditional_t<isHandle, Handle<T>, Optional<T&>>;

    AssetLoaderBase& loader;

    HYP_DEPRECATED static inline CastedType ExtractAssetValue(HypData& value)
    {
        if constexpr (isHandle)
        {
            if (Handle<T>* handlePtr = value.TryGet<Handle<T>>())
            {
                return *handlePtr;
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

// Formatter for AssetPath
namespace utilities {

template <class StringType>
struct Formatter<StringType, AssetPath>
{
    auto operator()(const AssetPath& value) const
    {
        return value.ToString();
    }
};

} // namespace utilities

} // namespace hyperion

