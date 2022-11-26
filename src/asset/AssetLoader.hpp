#ifndef HYPERION_V2_ASSET_LOADER_HPP
#define HYPERION_V2_ASSET_LOADER_HPP

#include <core/Core.hpp>
#include <core/Containers.hpp>
#include <core/Handle.hpp>
#include <core/OpaqueHandle.hpp>
#include <scene/Node.hpp>

#include <system/Debug.hpp>

#include "Loader.hpp"
#include "LoaderObject.hpp"

namespace hyperion::v2 {

class AssetManager;

// reference counted value (used internally and by serializer/deserializer)
using AssetValue = Variant<NodeProxy, AtomicRefCountedPtr<void>>;

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
    // using ResultType = std::conditional_t<std::is_base_of_v<EngineComponentBaseBase, T>, HandleBase, AtomicRefCountedPtr<void>>;
    // using CastedType = std::conditional_t<std::is_base_of_v<EngineComponentBaseBase, T>, Handle<T>, AtomicRefCountedPtr<T>>;

    static constexpr bool is_opaque_handle = has_opaque_handle_defined<T>;   //std::is_base_of_v<EngineComponentBaseBase, T>;

    using ResultType = AtomicRefCountedPtr<void>;
    using CastedType = std::conditional_t<is_opaque_handle, OpaqueHandle<T>, AtomicRefCountedPtr<T>>;


    AssetLoaderBase &loader;

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    static inline CastedType EmptyResult()
        { return CastedType(); }

    template <class Engine>
    CastedType Load(AssetManager &asset_manager, Engine *engine, const String &path, LoaderResult &out_result)
    {
        auto loaded_asset = loader.Load(asset_manager, path);
        out_result = loaded_asset.result;

        if (!loaded_asset.result) {
            return CastedType();
        }

        if (loaded_asset.value.template Is<UniquePtr<void>>()) {
            UniquePtr<void> &casted_result = loaded_asset.value.template Get<UniquePtr<void>>();
            
            return MakeCastedType<Engine>(engine, std::move(casted_result));
        } else if (loaded_asset.value.template Is<AssetValue>()) {
            AssetValue &as_ref_counted = loaded_asset.value.template Get<AssetValue>();

            if (as_ref_counted.template Is<ResultType>()) {
                if constexpr (is_opaque_handle) {
                    auto casted = as_ref_counted.template Get<ResultType>().template Cast<OpaqueHandle<T>>();

                    if (casted) {
                        return *casted;
                    }
                } else {
                    return as_ref_counted.template Get<ResultType>().template Cast<T>();
                }
            }
        } else {
            AssertThrowMsg(false, "Unhandled asset type");
        }

        return CastedType();
    }

    template <class Engine>
    static auto MakeCastedType(Engine *engine, UniquePtr<void> &&ptr) -> CastedType
    {
        if (ptr) {
            if constexpr (is_opaque_handle/*std::is_same_v<ResultType, HandleBase>*/) {
                auto casted = ptr.Cast<OpaqueHandle<T>>();

                if (casted) {
                    return *casted;
                }
                // return engine->template CreateHandle<T>(casted_result.Release());
            } else if (auto casted = ptr.Cast<T>()) {
                AtomicRefCountedPtr<T> ref_counted_ptr;
                ref_counted_ptr.Reset(casted.Release());
                return ref_counted_ptr;
            }
        }

        return CastedType();
    }

    template <class Engine>
    static auto MakeResultType(Engine *engine, UniquePtr<void> &&ptr) -> ResultType
    {
        if constexpr (is_opaque_handle/*std::is_base_of_v<EngineComponentBaseBase, T>*/) {
            // store OpaqueHandle<T> in AtomicRefCountedPtr<void>
            OpaqueHandle<T> handle = MakeCastedType(engine, std::move(ptr));

            return AtomicRefCountedPtr<OpaqueHandle<T>>::Construct(std::move(handle)).template Cast<void>();
        } else {
            // AtomicRefCountedPtr<void>
            return MakeCastedType(engine, std::move(ptr)).template Cast<void>();
        }
    }

    template <class Engine>
    static auto MakeResultType(Engine *engine, CastedType &&casted_value) -> ResultType
    {
        if constexpr (is_opaque_handle/*std::is_base_of_v<EngineComponentBaseBase, T>*/) {
            static_assert(std::is_same_v<CastedType, OpaqueHandle<T>>);
            return AtomicRefCountedPtr<OpaqueHandle<T>>::Construct(std::move(casted_value)).template Cast<void>();
        } else {
            // AtomicRefCountedPtr<void>
            return casted_value.template Cast<void>();
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
    NodeProxy Load(AssetManager &asset_manager, Engine *engine, const String &path, LoaderResult &out_result)
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
    static auto MakeCastedType(Engine *engine, UniquePtr<void> &&ptr) -> CastedType
    {
        auto casted_result = ptr.Cast<Node>();

        if (casted_result) {
            return NodeProxy(casted_result.Release());
        }

        return CastedType();
    }

    template <class Engine>
    static auto MakeResultType(Engine *engine, UniquePtr<void> &&ptr) -> ResultType
    {
        return MakeCastedType(engine, std::move(ptr));
    }

    template <class Engine>
    static auto MakeResultType(Engine *engine, CastedType &&casted_value) -> ResultType
    {
        return casted_value;
    }
};

} // namespace hyperion::v2

#endif