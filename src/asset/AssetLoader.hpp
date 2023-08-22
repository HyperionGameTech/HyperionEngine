#ifndef HYPERION_V2_ASSET_LOADER_HPP
#define HYPERION_V2_ASSET_LOADER_HPP

#include <core/Core.hpp>
#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <core/Handle.hpp>
#include <scene/Node.hpp>
#include <Constants.hpp>

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
    Variant<AnyPtr, AssetValue> value;

    LoadedAsset() = default;

    LoadedAsset(LoaderResult result)
        : result(std::move(result))
    {
    }

    LoadedAsset(LoaderResult result, AnyPtr &&value)
        : result(std::move(result)),
          value(std::move(value))
    {
    }

    LoadedAsset(LoaderResult result, AssetValue &&value)
        : result(std::move(result)),
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
struct AssetLoadResultWrapper;



template <class T, bool b>
struct InnerTypeImpl;

template <class T>
struct InnerTypeImpl<T, true>
{
    using type = typename AssetLoadResultWrapper<T>::type;
};

template <class T>
struct InnerTypeImpl<T, false>
{
    using type = T;
};

template <class T>
struct AssetLoaderWrapper
{
private:

public:
    static constexpr bool is_opaque_handle = has_opaque_handle_defined<T>;

    using InnerType = typename InnerTypeImpl<T, implementation_exists<AssetLoadResultWrapper<T>>>::type;

    using ResultType = AtomicRefCountedPtr<void>;
    using CastedType = std::conditional_t<is_opaque_handle, Handle<T>, AtomicRefCountedPtr< InnerType > >;

    AssetLoaderBase &loader;

    static inline CastedType ExtractAssetValue(AssetValue &value)
    {   
        if (value.Is<ResultType>()) {
            if constexpr (is_opaque_handle) {
                auto casted = value.Get<ResultType>().Cast<Handle<T>>();

                if (casted) {
                    return *casted;
                }
            } else {
                return value.Get<ResultType>().Cast<InnerType>();
            }
        }

        return EmptyResult();
    }

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    static inline CastedType EmptyResult()
        { return CastedType(); }

    CastedType Load(AssetManager &asset_manager, const String &path, LoaderResult &out_result)
    {
        auto loaded_asset = loader.Load(asset_manager, path);
        out_result = loaded_asset.result;

        if (!loaded_asset.result) {
            return EmptyResult();
        }

        if (loaded_asset.value.Is<AnyPtr>()) {
            AnyPtr &casted_result = loaded_asset.value.Get<AnyPtr>();
            
            return MakeCastedType(std::move(casted_result));
        } else if (loaded_asset.value.Is<AssetValue>()) {

            return ExtractAssetValue(loaded_asset.value.Get<AssetValue>());
        } else {
            AssertThrowMsg(false, "Unhandled variant type!");
        }

        return EmptyResult();
    }

    static auto MakeCastedType(AnyPtr &&ptr) -> CastedType
    {
        if (ptr) {
            if constexpr (is_opaque_handle) {
                auto casted = ptr.Cast<Handle<T>>();

                if (casted) {
                    return *casted;
                }
            } else if (UniquePtr<InnerType> casted = ptr.Cast<InnerType>()) {
                AtomicRefCountedPtr<InnerType> ref_counted_ptr;
                ref_counted_ptr.Reset(casted.Release());
                return ref_counted_ptr;
            }
        }

        return EmptyResult();
    }

    static auto MakeResultType(AnyPtr &&ptr) -> ResultType
    {
        if constexpr (is_opaque_handle) {
            Handle<T> handle = MakeCastedType(std::move(ptr));

            return AtomicRefCountedPtr<Handle<T>>::Construct(std::move(handle)).template Cast<void>();
        } else {
            // AtomicRefCountedPtr<void>
            return MakeCastedType(std::move(ptr)).template Cast<void>();
        }
    }

    static auto MakeResultType(CastedType &&casted_value) -> ResultType
    {
        if constexpr (is_opaque_handle) {
            static_assert(std::is_same_v<CastedType, Handle<T>>);
            return AtomicRefCountedPtr<Handle<T>>::Construct(std::move(casted_value)).template Cast<void>();
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

    static inline CastedType ExtractAssetValue(AssetValue &value)
    {
        if (value.Is<ResultType>()) {
            return value.Get<ResultType>();
        }

        return CastedType();
    }

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    static inline NodeProxy EmptyResult()
        { return NodeProxy(); }

    NodeProxy Load(AssetManager &asset_manager, const String &path, LoaderResult &out_result)
    {
        auto loaded_asset = loader.Load(asset_manager, path);
        out_result = loaded_asset.result;

        if (!loaded_asset.result) {
            return CastedType();
        }

        if (loaded_asset.value.Is<AnyPtr>()) {
            auto casted_result = loaded_asset.value.Get<AnyPtr>().Cast<Node>();
            
            return MakeCastedType(std::move(casted_result));
        } else if (loaded_asset.value.Is<AssetValue>()) {
            auto &as_ref_counted = loaded_asset.value.Get<AssetValue>();

            if (as_ref_counted.Is<ResultType>()) {
                return as_ref_counted.Get<ResultType>();
            }
        }

        return CastedType();
    }

    static auto MakeCastedType(AnyPtr &&ptr) -> CastedType
    {
        auto casted_result = ptr.Cast<Node>();

        if (casted_result) {
            return NodeProxy(casted_result.Release());
        }

        return CastedType();
    }

    static auto MakeResultType(AnyPtr &&ptr) -> ResultType
    {
        return MakeCastedType(std::move(ptr));
    }

    static auto MakeResultType(CastedType &&casted_value) -> ResultType
    {
        return casted_value;
    }
};

} // namespace hyperion::v2

#endif