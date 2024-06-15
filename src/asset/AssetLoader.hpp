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

// reference counted value (used internally and by serializer/deserializer)
using AssetValue = Any;

struct LoadedAsset
{
    LoaderResult    result;
    AssetValue      value;

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

    LoadedAsset(const LoadedAsset &other)               = delete;
    LoadedAsset &operator=(const LoadedAsset &other)    = delete;
    LoadedAsset(LoadedAsset &&other) noexcept           = default;
    LoadedAsset &operator=(LoadedAsset &&other)         = default;
    ~LoadedAsset()                                      = default;
};


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

    static inline CastedType EmptyResult()
        { return CastedType(); }

    CastedType Load(AssetManager &asset_manager, const String &path, LoaderResult &out_result)
    {
        LoadedAsset loaded_asset = loader.Load(asset_manager, path);
        out_result = loaded_asset.result;

        if (!loaded_asset.result) {
            return EmptyResult();
        }
        
        return ExtractAssetValue(loaded_asset.value);
    }

    static auto MakeResultType(CastedType &&casted_value) -> ResultType
    {
        return Any(std::move(casted_value));
    }
};

template <class T>
struct AssetLoaderWrapper<RC<T>>
{
public:
    using ResultType = Any;
    using CastedType = RC<T>;

    AssetLoaderBase &loader;

    static inline CastedType ExtractAssetValue(AssetValue &value)
    {   
        return value.Get<RC<T>>();
    }

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    static inline CastedType EmptyResult()
        { return CastedType(); }

    CastedType Load(AssetManager &asset_manager, const String &path, LoaderResult &out_result)
    {
        LoadedAsset loaded_asset = loader.Load(asset_manager, path);
        out_result = loaded_asset.result;

        if (!loaded_asset.result) {
            return EmptyResult();
        }
        
        return ExtractAssetValue(loaded_asset.value);
    }

    static auto MakeResultType(CastedType &&casted_value) -> ResultType
    {
        return Any(std::move(casted_value));
    }
};

template <>
struct AssetLoaderWrapper<Node>
{
    using ResultType = Any;
    using CastedType = NodeProxy;

    AssetLoaderBase &loader;

    static inline CastedType ExtractAssetValue(AssetValue &value)
    {
        NodeProxy *result = value.TryGet<NodeProxy>();

        if (!result) {
            return EmptyResult();
        }

        if (result->IsValid()) {
            (*result)->SetScene(nullptr); // sets scene to be the "detached" scene for the current thread we're on.
        }

        return *result;
    }

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    static inline CastedType EmptyResult()
        { return CastedType(); }

    CastedType Load(AssetManager &asset_manager, const String &path, LoaderResult &out_result)
    {
        LoadedAsset loaded_asset = loader.Load(asset_manager, path);
        out_result = loaded_asset.result;

        if (!loaded_asset.result) {
            return EmptyResult();
        }
        
        return ExtractAssetValue(loaded_asset.value);
    }

    static auto MakeResultType(CastedType &&casted_value) -> ResultType
    {
        return Any(std::move(casted_value));
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