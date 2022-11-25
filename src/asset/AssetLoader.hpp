#ifndef HYPERION_V2_ASSET_LOADER_HPP
#define HYPERION_V2_ASSET_LOADER_HPP

#include <core/Core.hpp>
#include <core/Containers.hpp>
#include <core/Handle.hpp>
#include <scene/Node.hpp>

#include <system/Debug.hpp>

#include "Loader.hpp"
#include "LoaderObject.hpp"

namespace hyperion::v2 {

class AssetManager;

// reference counted value (used internally and by serializer/deserializer)
using AssetValue = Variant<NodeProxy, AtomicRefCountedPtr<void>, HandleBase>;

struct LoadedAsset
{
    LoaderResult result;
    Variant<UniquePtr<void>, AssetValue> value;

    LoadedAsset() = default;

    LoadedAsset(LoaderResult result)
        : result(result)
    {
    }

    LoadedAsset(LoaderResult result, UniquePtr<void> &&value)
        : result(result),
          value(std::move(value))
    {
    }

    LoadedAsset(LoaderResult result, AssetValue &&value)
        : result(result),
          value(std::move(value))
    {
    }

    LoadedAsset(const LoadedAsset &other) = delete;
    LoadedAsset &operator=(const LoadedAsset &other) = delete;
    LoadedAsset(LoadedAsset &&other) noexcept = default;
    LoadedAsset &operator=(LoadedAsset &&other) = default;
    ~LoadedAsset() = default;
};

class AssetLoaderBase
{
public:
    virtual ~AssetLoaderBase() = default;

    virtual LoadedAsset Load(AssetManager &asset_manager, const String &path) const = 0;
};

template <class T>
struct AssetLoaderWrapper
{
    using ResultType = std::conditional_t<std::is_base_of_v<EngineComponentBaseBase, T>, HandleBase, AtomicRefCountedPtr<void>>;
    using CastedType = std::conditional_t<std::is_base_of_v<EngineComponentBaseBase, T>, Handle<T>, AtomicRefCountedPtr<T>>;

    AssetLoaderBase &loader;

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    static inline CastedType EmptyResult()
        { return CastedType(); }

    template <class Engine>
    CastedType Load(AssetManager &asset_manager,  const String &path, LoaderResult &out_result)
    {
        auto loaded_asset = loader.Load(asset_manager, path);
        out_result = loaded_asset.result;

        if (!loaded_asset.result) {
            return CastedType();
        }

        if (loaded_asset.value.template Is<UniquePtr<void>>()) {
            auto casted_result = loaded_asset.value.template Get<UniquePtr<void>>().template Cast<T>();
            
            return MakeCastedType<Engine>(engine, std::move(casted_result));
        } else if (loaded_asset.value.template Is<AssetValue>()) {
            auto &as_ref_counted = loaded_asset.value.template Get<AssetValue>();

            if (as_ref_counted.template Is<ResultType>()) {
                return as_ref_counted.template Get<ResultType>().template Cast<T>();
            }
        }

        return CastedType();
    }

    template <class Engine>
    static auto MakeCastedType( UniquePtr<T> &&casted_result) -> CastedType
    {
        if (casted_result) {
            if constexpr (std::is_same_v<ResultType, HandleBase>) {
                return engine->template CreateHandle<T>(casted_result.Release());
            } else {
                AtomicRefCountedPtr<T> ptr;
                ptr.Reset(casted_result.Release());
                return ptr;
            }
        }

        return CastedType();
    }

    template <class Engine>
    static auto MakeResultType( UniquePtr<T> &&casted_result) -> ResultType
    {
        if constexpr (std::is_base_of_v<EngineComponentBaseBase, T>) {
            return MakeCastedType(engine, std::move(casted_result));
        } else {
            return MakeCastedType(engine, std::move(casted_result)).template Cast<void>();
        }
    }
};

template <>
struct AssetLoaderWrapper<Node>
{
    using ResultType = NodeProxy;
    using CastedType = NodeProxy;

    AssetLoaderBase &loader;

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    static inline NodeProxy EmptyResult()
        { return NodeProxy(); }

    template <class Engine>
    NodeProxy Load(AssetManager &asset_manager,  const String &path, LoaderResult &out_result)
    {
        auto loaded_asset = loader.Load(asset_manager, path);
        out_result = loaded_asset.result;

        if (!loaded_asset.result) {
            return CastedType();
        }

        if (loaded_asset.value.template Is<UniquePtr<void>>()) {
            auto casted_result = loaded_asset.value.template Get<UniquePtr<void>>().template Cast<Node>();
            
            return MakeCastedType<Engine>(engine, std::move(casted_result));
        } else if (loaded_asset.value.template Is<AssetValue>()) {
            auto &as_ref_counted = loaded_asset.value.template Get<AssetValue>();

            if (as_ref_counted.template Is<ResultType>()) {
                return as_ref_counted.template Get<ResultType>();
            }
        }

        return CastedType();
    }

    template <class Engine>
    static auto MakeCastedType( UniquePtr<Node> &&casted_result) -> CastedType
    {
        if (casted_result) {
            return NodeProxy(casted_result.Release());
        }

        return CastedType();
    }

    template <class Engine>
    static auto MakeResultType( UniquePtr<Node> &&casted_result) -> ResultType
    {
        return MakeCastedType(engine, std::move(casted_result));
    }
};

} // namespace hyperion::v2

#endif