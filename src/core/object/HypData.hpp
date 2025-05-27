/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_DATA_HPP
#define HYPERION_CORE_HYP_DATA_HPP

#include <core/Defines.hpp>

#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Result.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/ByteBuffer.hpp>

#include <core/serialization/fbom/FBOMResult.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/FBOM.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

struct HypData;

class Node;
class Entity;

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

template <class T, class T2 = void>
struct HypDataHelper;

template <class T, class T2 = void>
struct HypDataHelperDecl;

namespace detail {

template <class T, class ConvertibleFromTuple>
struct HypDataTypeChecker_Tuple;

template <class ReturnType, class T, class ConvertibleFromTuple>
struct HypDataGetter_Tuple;

template <class T, bool IsConst>
struct HypDataGetReturnTypeHelper
{
    using Type = decltype(std::declval<HypDataHelper<T>>().Get(*std::declval<std::add_pointer_t<std::conditional_t<IsConst, std::add_const_t<T>, T>>>()));
};

} // namespace detail

using HypDataSerializeFunction = fbom::FBOMResult (*)(const HypData& data, fbom::FBOMData& out);

namespace detail {

template <class T>
struct DefaultHypDataSerializeFunction;

template <>
struct DefaultHypDataSerializeFunction<void>
{
    static inline fbom::FBOMResult Serialize(const HypData& data, fbom::FBOMData& out)
    {
        out = fbom::FBOMData();

        return { fbom::FBOMResult::FBOM_ERR, "No serialization function provided" };
    }
};

} // namespace detail

/*! \brief A type-safe union that can store multiple different types of run-time data, abstracting away internal engine structures such as Handle<T>, RC<T>, etc.
 *  Providing a unified way of accessing the data via Get<T>() and TryGet<T>() methods.
 *  \note Used in serialization, reflection, scripting, and other systems where data needs to be stored in a generic way.
 */
struct HypData
{
    using VariantType = Variant<
        int8,
        int16,
        int32,
        int64,
        uint8,
        uint16,
        uint32,
        uint64,
        char,
        float,
        double,
        bool,
        void*,
        IDBase,
        AnyHandle,
        RC<void>,
        AnyRef,
        Any>;

    template <class T>
    static constexpr bool can_store_directly =
        /* Fundamental types - can be stored inline */
        std::is_same_v<T, int8> || std::is_same_v<T, int16> | std::is_same_v<T, int32> | std::is_same_v<T, int64>
        || std::is_same_v<T, uint8> || std::is_same_v<T, uint16> || std::is_same_v<T, uint32> || std::is_same_v<T, uint64>
        || std::is_same_v<T, char>
        || std::is_same_v<T, float> || std::is_same_v<T, double>
        || std::is_same_v<T, bool>
        || std::is_same_v<T, void*>

        /*! All ID<T> are stored as IDBase */
        || std::is_base_of_v<IDBase, T>

        /*! Handle<T> gets stored as AnyHandle, which holds TypeID for conversion */
        || std::is_base_of_v<HandleBase, T> || std::is_same_v<T, AnyHandle>

        /*! RC<T> gets stored as RC<void> and can be converted back */
        || std::is_base_of_v<typename RC<void>::RefCountedPtrBase, T>

        /*! Pointers are stored as AnyRef which holds TypeID for conversion */
        || std::is_same_v<T, AnyRef> || std::is_pointer_v<T>

        || std::is_same_v<T, Any>;

    VariantType value;
    HypDataSerializeFunction serialize_function;

    HypData()
        : serialize_function(&detail::DefaultHypDataSerializeFunction<void>::Serialize)
    {
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<T, HypData>>>
    explicit HypData(T&& value)
        : HypData()
    {
        HYP_NAMED_SCOPE("Construct HypData from T");

        HypDataHelper<NormalizedType<T>> {}.Set(*this, std::forward<T>(value));
    }

    HypData(const HypData& other) = delete;
    HypData& operator=(const HypData& other) = delete;

    HypData(HypData&& other) noexcept
        : value(std::move(other.value)),
          serialize_function(other.serialize_function)
    {
        other.serialize_function = &detail::DefaultHypDataSerializeFunction<void>::Serialize;
    }

    HypData& operator=(HypData&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        value = std::move(other.value);
        serialize_function = other.serialize_function;
        other.serialize_function = &detail::DefaultHypDataSerializeFunction<void>::Serialize;

        return *this;
    }

    ~HypData() = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return value.IsValid();
    }

    HYP_FORCE_INLINE bool IsNull() const
    {
        return !ToRef().HasValue();
    }

    HYP_FORCE_INLINE TypeID GetTypeID() const
    {
        return ToRef().GetTypeID();
    }

    HYP_FORCE_INLINE void Reset()
    {
        value.Reset();
    }

    HYP_FORCE_INLINE AnyRef ToRef()
    {
        HYP_SCOPE;

        if (!IsValid())
        {
            return AnyRef();
        }

        if (AnyHandle* any_handle_ptr = value.TryGet<AnyHandle>())
        {
            return any_handle_ptr->ToRef();
        }

        if (RC<void>* rc_ptr = value.TryGet<RC<void>>())
        {
            return rc_ptr->ToRef();
        }

        if (AnyRef* any_ref_ptr = value.TryGet<AnyRef>())
        {
            return *any_ref_ptr;
        }

        if (Any* any_ptr = value.TryGet<Any>())
        {
            return any_ptr->ToRef();
        }

        return AnyRef(value.GetTypeID(), value.GetPointer());
    }

    HYP_FORCE_INLINE AnyRef ToRef() const
    {
        return const_cast<HypData*>(this)->ToRef();
    }

    template <class T>
    HYP_FORCE_INLINE bool Is(bool strict = false) const
    {
        HYP_SCOPE;

        if constexpr (std::is_same_v<T, HypData>)
        {
            return true;
        }
        else
        {
            if (!value.IsValid())
            {
                return false;
            }

            if (strict)
            {
                return detail::HypDataTypeChecker_Tuple<T, Tuple<>> {}(value);
            }

            return detail::HypDataTypeChecker_Tuple<T, typename HypDataHelper<T>::ConvertibleFrom> {}(value);
        }
    }

    template <class T>
    HYP_FORCE_INLINE auto Get() -> std::conditional_t<std::is_same_v<T, HypData>, HypData&, typename detail::HypDataGetReturnTypeHelper<T, false>::Type>
    {
        HYP_SCOPE;

        if constexpr (std::is_same_v<T, HypData>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename detail::HypDataGetReturnTypeHelper<T, false>::Type;

            Optional<ReturnType> result_value;

            detail::HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getter_instance {};

            AssertThrowMsg(getter_instance(value, result_value),
                "Failed to invoke HypData Get method with T = %s - Mismatched types or T could not be converted to the held type (current TypeID = %u)",
                TypeName<T>().Data(),
                GetTypeID().Value());

            return *result_value;
        }
    }

    template <class T>
    HYP_FORCE_INLINE auto Get() const -> std::conditional_t<std::is_same_v<T, HypData>, const HypData&, typename detail::HypDataGetReturnTypeHelper<T, true>::Type>
    {
        HYP_SCOPE;

        if constexpr (std::is_same_v<T, HypData>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename detail::HypDataGetReturnTypeHelper<T, true>::Type;

            Optional<ReturnType> result_value;

            detail::HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getter_instance {};

            AssertThrowMsg(getter_instance(value, result_value),
                "Failed to invoke HypData Get method with T = %s - Mismatched types or T could not be converted to the held type (current TypeID = %u)",
                TypeName<T>().Data(),
                GetTypeID().Value());

            return *result_value;
        }
    }

    template <class T>
    HYP_FORCE_INLINE auto TryGet() -> Optional<std::conditional_t<std::is_same_v<T, HypData>, HypData&, typename detail::HypDataGetReturnTypeHelper<T, false>::Type>>
    {
        HYP_SCOPE;

        if constexpr (std::is_same_v<T, HypData>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename detail::HypDataGetReturnTypeHelper<T, false>::Type;

            Optional<ReturnType> result_value;

            detail::HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getter_instance {};
            getter_instance(value, result_value);

            return result_value;
        }
    }

    template <class T>
    HYP_FORCE_INLINE auto TryGet() const -> Optional<std::conditional_t<std::is_same_v<T, HypData>, const HypData&, typename detail::HypDataGetReturnTypeHelper<T, true>::Type>>
    {
        HYP_SCOPE;

        if constexpr (std::is_same_v<T, HypData>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename detail::HypDataGetReturnTypeHelper<T, true>::Type;

            Optional<ReturnType> result_value;

            detail::HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getter_instance {};
            getter_instance(value, result_value);

            return result_value;
        }
    }

    /*! \brief Serialize this instance to an FBOMData object.
     *  \param out The FBOMData object to serialize to.
     *  \return The result of the serialization operation.
     */
    fbom::FBOMResult Serialize(fbom::FBOMData& out) const
    {
        AssertThrow(serialize_function != nullptr);

        return serialize_function(*this, out);
    }

    template <class T>
    static fbom::FBOMResult Serialize(T&& value, fbom::FBOMData& out)
    {
        return HypDataHelper<NormalizedType<T>>::Serialize(value, out);
    }

    template <class T>
    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, T& out)
    {
        HypData out_data;

        if (fbom::FBOMResult err = HypDataHelper<NormalizedType<T>>::Deserialize(context, data, out_data))
        {
            return err;
        }

        out = std::move(out_data.Get<T>());

        return {};
    }

    template <class T>
    void Set_Internal(T&& value)
    {
        this->value.Set<NormalizedType<T>>(std::forward<T>(value));
        serialize_function = &detail::DefaultHypDataSerializeFunction<NormalizedType<T>>::Serialize;
    }
};

namespace detail {

template <class T>
struct DefaultHypDataSerializeFunction
{
    static inline fbom::FBOMResult Serialize(const HypData& data, fbom::FBOMData& out)
    {
        if (fbom::FBOMResult err = HypDataHelper<NormalizedType<T>>::Serialize(data.Get<NormalizedType<T>>(), out))
        {
            return err;
        }

        return {};
    }
};

} // namespace detail

#pragma region HypDataMarshalHelper

struct HypDataMarshalHelper
{
    static HYP_API fbom::FBOMResult NoMarshalRegistered(ANSIStringView type_name);

    template <class T>
    static inline fbom::FBOMResult Serialize(const T& value, fbom::FBOMData& out_data)
    {
        using Normalized = NormalizedType<T>;

        HYP_SCOPE;

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<Normalized>());

        if (!marshal)
        {
            return NoMarshalRegistered(TypeName<Normalized>());
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(ConstAnyRef(value), object))
        {
            return err;
        }

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }

    template <class T>
    static inline fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal)
        {
            return NoMarshalRegistered(TypeName<T>());
        }

        if (!data)
        {
            return { fbom::FBOMResult::FBOM_ERR, "Cannot deserialize unset value" };
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return fbom::FBOMResult::FBOM_OK;
    }
};

#pragma endregion HypDataMarshalHelper

namespace detail {
template <class T>
struct HypDataPlaceholderSerializedType;
} // namespace detail

template <class T>
struct HypDataHelperDecl<T, std::enable_if_t<std::is_fundamental_v<T>>>
{
};

template <class T>
struct HypDataHelper<T, std::enable_if_t<std::is_fundamental_v<T>>>
{
    using StorageType = T;
    using ConvertibleFrom = Tuple<
        int8,
        int16,
        int32,
        int64,
        uint8,
        uint16,
        uint32,
        uint64,
        char,
        float,
        double,
        bool>;

    HYP_FORCE_INLINE bool Is(T value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, T>>>
    HYP_FORCE_INLINE bool Is(OtherT value) const
    {
        return std::is_fundamental_v<OtherT>;
    }

    HYP_FORCE_INLINE constexpr T Get(T value) const
    {
        return value;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, T>>>
    HYP_FORCE_INLINE constexpr T Get(OtherT value) const
    {
        return static_cast<T>(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, T value) const
    {
        hyp_data.Set_Internal(value);
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(T value, fbom::FBOMData& out)
    {
        out = fbom::FBOMData(value);

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        T value;

        if (fbom::FBOMResult err = data.Read(&value))
        {
            return err;
        }

        out = HypData(value);

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<T, std::enable_if_t<std::is_enum_v<T>>>
{
};

template <class T>
struct HypDataHelper<T, std::enable_if_t<std::is_enum_v<T>>> : HypDataHelper<std::underlying_type_t<T>>
{
    HYP_FORCE_INLINE bool Is(T value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE constexpr bool Is(std::underlying_type_t<T>) const
    {
        return true;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, T> && !std::is_same_v<OtherT, std::underlying_type_t<T>>>>
    HYP_FORCE_INLINE constexpr bool Is(OtherT value) const
    {
        return std::is_fundamental_v<OtherT>;
    }

    HYP_FORCE_INLINE constexpr T Get(T value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr T Get(std::underlying_type<T> value) const
    {
        return static_cast<T>(value);
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, T> && !std::is_same_v<OtherT, std::underlying_type<T>>>>
    HYP_FORCE_INLINE constexpr T Get(OtherT value) const
    {
        return static_cast<T>(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, T value) const
    {
        HypDataHelper<std::underlying_type_t<T>>::Set(hyp_data, static_cast<std::underlying_type_t<T>>(value));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(T value, fbom::FBOMData& out)
    {
        out = fbom::FBOMData(static_cast<std::underlying_type_t<T>>(value));

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        std::underlying_type_t<T> value;

        if (fbom::FBOMResult err = data.Read(&value))
        {
            return err;
        }

        out = HypData(value);

        return fbom::FBOMResult::FBOM_OK;
    }
};

/* void pointer specialization - only meant for runtime, non-serializable. */
template <>
struct HypDataHelperDecl<void*>
{
};

template <>
struct HypDataHelper<void*>
{
    using StorageType = void*;
    using ConvertibleFrom = Tuple<AnyRef, AnyHandle, RC<void>>;

    HYP_FORCE_INLINE bool Is(void* value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        return true;
    }

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        return true;
    }

    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        return true;
    }

    HYP_FORCE_INLINE constexpr void* Get(void* value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void* Get(const AnyRef& value) const
    {
        return value.GetPointer();
    }

    HYP_FORCE_INLINE void* Get(const AnyHandle& value) const
    {
        return value.ToRef().GetPointer();
    }

    HYP_FORCE_INLINE void* Get(const RC<void>& value) const
    {
        return value.Get();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, void* value) const
    {
        hyp_data.Set_Internal(value);
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(void* value, fbom::FBOMData& out)
    {
        return { fbom::FBOMResult::FBOM_ERR, "Cannot serialize a user pointer!" };
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        return { fbom::FBOMResult::FBOM_ERR, "Cannot deserialize a user pointer!" };
    }
};

template <class T>
struct HypDataHelperDecl<EnumFlags<T>>
{
};

template <class T>
struct HypDataHelper<EnumFlags<T>> : HypDataHelper<typename EnumFlags<T>::UnderlyingType>
{
    using ConvertibleFrom = Tuple<typename EnumFlags<T>::UnderlyingType>;

    HYP_FORCE_INLINE bool Is(typename EnumFlags<T>::UnderlyingType value) const
    {
        return true;
    }

    HYP_FORCE_INLINE constexpr EnumFlags<T> Get(EnumFlags<T> value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr EnumFlags<T> Get(typename EnumFlags<T>::UnderlyingType value) const
    {
        return static_cast<EnumFlags<T>>(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, EnumFlags<T> value) const
    {
        HypDataHelper<typename EnumFlags<T>::UnderlyingType>::Set(hyp_data, static_cast<typename EnumFlags<T>::UnderlyingType>(value));
    }
};

template <>
struct HypDataHelperDecl<IDBase>
{
};

template <>
struct HypDataHelper<IDBase>
{
    using StorageType = IDBase;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const IDBase& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE constexpr IDBase& Get(IDBase& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const IDBase& Get(const IDBase& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const IDBase& value) const
    {
        hyp_data.Set_Internal(value);
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(IDBase value, fbom::FBOMData& out_data)
    {
        out_data = fbom::FBOMData::FromStruct<IDBase>(value);

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        IDBase value;

        if (fbom::FBOMResult err = data.ReadStruct<IDBase>(&value))
        {
            return err;
        }

        out = HypData(value);

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<ID<T>>
{
};

template <class T>
struct HypDataHelper<ID<T>> : HypDataHelper<IDBase>
{
    using ConvertibleFrom = Tuple<AnyHandle>;

    HYP_FORCE_INLINE bool Is(const IDBase& value) const
    {
        return true; // can't do anything more to check as IDBase doesn't hold type info.
    }

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE ID<T> Get(IDBase value) const
    {
        return ID<T>(value);
    }

    HYP_FORCE_INLINE ID<T> Get(const AnyHandle& value) const
    {
        return ID<T>(value.GetID());
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const ID<T>& value) const
    {
        HypDataHelper<IDBase>::Set(hyp_data, static_cast<const IDBase&>(value));
    }
};

template <>
struct HypDataHelperDecl<AnyHandle>
{
};

template <>
struct HypDataHelper<AnyHandle>
{
    using StorageType = AnyHandle;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE AnyHandle& Get(AnyHandle& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const AnyHandle& Get(const AnyHandle& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const AnyHandle& value) const
    {
        hyp_data.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, AnyHandle&& value) const
    {
        hyp_data.Set_Internal(std::move(value));
    }

    static fbom::FBOMResult Serialize(const AnyHandle& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        if (!value.IsValid())
        {
            // unset
            out_data = fbom::FBOMData();

            return fbom::FBOMResult::FBOM_OK;
        }

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(value.GetTypeID());

        if (!marshal)
        {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(value.ToRef(), object))
        {
            return err;
        }

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        if (!data)
        {
            out = HypData(AnyHandle {});

            return fbom::FBOMResult::FBOM_OK;
        }

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(data.GetType().GetNativeTypeID());

        if (!marshal)
        {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<Handle<T>>
{
};

template <class T>
struct HypDataHelper<Handle<T>> : HypDataHelper<AnyHandle>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        return value.GetTypeID() == TypeID::ForType<T>();
    }

    HYP_FORCE_INLINE Handle<T>& Get(AnyHandle& value) const
    {
        return *reinterpret_cast<Handle<T>*>(&value);
    }

    HYP_FORCE_INLINE const Handle<T>& Get(const AnyHandle& value) const
    {
        return *reinterpret_cast<const Handle<T>*>(&value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const Handle<T>& value) const
    {
        HypDataHelper<AnyHandle>::Set(hyp_data, AnyHandle(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, Handle<T>&& value) const
    {
        HypDataHelper<AnyHandle>::Set(hyp_data, AnyHandle(std::move(value)));
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal)
        {
            HYP_BREAKPOINT;
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
        }

        if (!data)
        {
            out = HypData(Handle<T> {});

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <>
struct HypDataHelperDecl<RC<void>>
{
};

template <>
struct HypDataHelper<RC<void>>
{
    using StorageType = RC<void>;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE RC<void>& Get(RC<void>& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const RC<void>& Get(const RC<void>& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const RC<void>& value) const
    {
        hyp_data.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, RC<void>&& value) const
    {
        hyp_data.Set_Internal(std::move(value));
    }

    static fbom::FBOMResult Serialize(const RC<void>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(value.GetTypeID());

        if (!marshal)
        {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        if (!value)
        {
            // unset
            out_data = fbom::FBOMData();

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(value.ToRef(), object))
        {
            return err;
        }

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<RC<T>, std::enable_if_t<!std::is_void_v<T>>>
{
};

template <class T>
struct HypDataHelper<RC<T>, std::enable_if_t<!std::is_void_v<T>>> : HypDataHelper<RC<void>>
{
    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE RC<T>& Get(RC<void>& value) const
    {
        return *reinterpret_cast<RC<T>*>(&value);
    }

    HYP_FORCE_INLINE const RC<T>& Get(const RC<void>& value) const
    {
        return *reinterpret_cast<const RC<T>*>(&value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const RC<T>& value) const
    {
        HypDataHelper<RC<void>>::Set(hyp_data, value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, RC<T>&& value) const
    {
        HypDataHelper<RC<void>>::Set(hyp_data, std::move(value));
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal)
        {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data)
        {
            out = HypData(RC<T> {});

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <>
struct HypDataHelperDecl<AnyRef>
{
};

template <>
struct HypDataHelper<AnyRef>
{
    using StorageType = AnyRef;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE AnyRef& Get(AnyRef& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const AnyRef& Get(const AnyRef& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const AnyRef& value) const
    {
        hyp_data.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, AnyRef&& value) const
    {
        hyp_data.Set_Internal(std::move(value));
    }

    static fbom::FBOMResult Serialize(const AnyRef& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(value.GetTypeID());

        if (!marshal)
        {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        if (!value.HasValue())
        {
            // unset
            out_data = fbom::FBOMData();

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(value, object))
        {
            return err;
        }

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<T*, std::enable_if_t<!is_const_pointer<T*> && !std::is_same_v<T*, void*>>>
{
};

template <class T>
struct HypDataHelper<T*, std::enable_if_t<!is_const_pointer<T*> && !std::is_same_v<T*, void*>>> : HypDataHelper<AnyRef>
{
    using ConvertibleFrom = Tuple<AnyHandle, RC<void>>;

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        return value.Is<T>();
    }

    // HYP_FORCE_INLINE bool Is(const Any &value) const
    // {
    //     return value.Is<T>();
    // }

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE T* Get(const AnyRef& value) const
    {
        AssertThrow(value.Is<T>());

        return static_cast<T*>(value.GetPointer());
    }

    // T *Get(const Any &value) const
    // {
    //     AssertThrow(value.Is<T>());

    //     return value.Get<T>();
    // }

    HYP_FORCE_INLINE T* Get(const AnyHandle& value) const
    {
        AssertThrow(value.Is<T>());

        return value.TryGet<T>();
    }

    HYP_FORCE_INLINE T* Get(const RC<void>& value) const
    {
        AssertThrow(value.Is<T>());

        return value.CastUnsafe_Internal<T>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, T* value) const
    {
        HypDataHelper<AnyRef>::Set(hyp_data, AnyRef(TypeID::ForType<T>(), value));
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal)
        {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data)
        {
            out = HypData(static_cast<T*>(nullptr));

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<const T*, std::enable_if_t<!std::is_same_v<T*, void*>>>
{
};

template <class T>
struct HypDataHelper<const T*, std::enable_if_t<!std::is_same_v<T*, void*>>> : HypDataHelper<T*>
{
    HYP_FORCE_INLINE const T* Get(const ConstAnyRef& value) const
    {
        return HypDataHelper<T*>::Get(AnyRef(value.GetTypeID(), const_cast<void*>(value.GetPointer())));
    }

    HYP_FORCE_INLINE const T* Get(const AnyRef& value) const
    {
        return HypDataHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE const T* Get(const AnyHandle& value) const
    {
        return HypDataHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE const T* Get(const RC<void>& value) const
    {
        return HypDataHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const T* value) const
    {
        HypDataHelper<T*>::Set(hyp_data, const_cast<T*>(value));
    }
};

template <>
struct HypDataHelperDecl<Any>
{
};

template <>
struct HypDataHelper<Any>
{
    using StorageType = Any;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE Any& Get(Any& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const Any& Get(const Any& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, Any&& value) const
    {
        hyp_data.Set_Internal(std::move(value));
    }

    static fbom::FBOMResult Serialize(const Any& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(value.GetTypeID());

        if (!marshal)
        {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        if (!value.HasValue())
        {
            // unset
            out_data = fbom::FBOMData();

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(value.ToRef(), object))
        {
            return err;
        }

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <int StringType>
struct HypDataHelperDecl<containers::detail::String<StringType>>
{
};

template <int StringType>
struct HypDataHelper<containers::detail::String<StringType>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<containers::detail::String<StringType>>();
    }

    HYP_FORCE_INLINE containers::detail::String<StringType>& Get(Any& value) const
    {
        return value.Get<containers::detail::String<StringType>>();
    }

    HYP_FORCE_INLINE const containers::detail::String<StringType>& Get(const Any& value) const
    {
        return value.Get<containers::detail::String<StringType>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const containers::detail::String<StringType>& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<containers::detail::String<StringType>>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, containers::detail::String<StringType>&& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<containers::detail::String<StringType>>(std::move(value)));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(const containers::detail::String<StringType>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        out_data = fbom::FBOMData::FromString(value);

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        containers::detail::String<StringType> result;

        if (fbom::FBOMResult err = data.ReadString(result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <int StringType>
struct HypDataHelperDecl<utilities::detail::StringView<StringType>>
{
};

template <int StringType>
struct HypDataHelper<utilities::detail::StringView<StringType>> : HypDataHelper<containers::detail::String<StringType>>
{
    using ConvertibleFrom = Tuple<containers::detail::String<StringType>>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return HypDataHelper<containers::detail::String<StringType>>::Is(value);
    }

    HYP_FORCE_INLINE utilities::detail::StringView<StringType> Get(const Any& value) const
    {
        return HypDataHelper<containers::detail::String<StringType>>::Get(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const utilities::detail::StringView<StringType>& value) const
    {
        HypDataHelper<containers::detail::String<StringType>>::Set(hyp_data, value);
    }
};

template <>
struct HypDataHelperDecl<FilePath>
{
};

template <>
struct HypDataHelper<FilePath> : HypDataHelper<String>
{
    HYP_FORCE_INLINE FilePath Get(const Any& value) const
    {
        return value.Get<String>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const FilePath& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<String>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, FilePath&& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<String>(std::move(value)));
    }
};

template <>
struct HypDataHelperDecl<Name>
{
};

template <>
struct HypDataHelper<Name> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<ANSIString>();
    }

    HYP_FORCE_INLINE Name Get(const Any& value) const
    {
        return CreateNameFromDynamicString(value.Get<ANSIString>());
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, Name value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<ANSIString>(value.LookupString()));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(const Name& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        out_data = fbom::FBOMData::FromString(ANSIString(value.LookupString()));

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        ANSIString result;

        if (fbom::FBOMResult err = data.ReadString(result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<WeakName>
{
};

template <>
struct HypDataHelper<WeakName> : HypDataHelper<Name>
{
};

template <class T, class AllocatorType>
struct HypDataHelperDecl<Array<T, AllocatorType>, std::enable_if_t<!std::is_const_v<T>>>
{
};

template <class T, class AllocatorType>
struct HypDataHelper<Array<T, AllocatorType>, std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Array<T, AllocatorType>>();
    }

    HYP_FORCE_INLINE Array<T, AllocatorType>& Get(const Any& value) const
    {
        return value.Get<Array<T, AllocatorType>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const Array<T, AllocatorType>& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Array<T, AllocatorType>>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, Array<T, AllocatorType>&& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Array<T, AllocatorType>>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(const Array<T, AllocatorType>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(detail::HypDataPlaceholderSerializedType<T>::Get()));

            return fbom::FBOMResult::FBOM_OK;
        }

        Array<fbom::FBOMData> elements;
        elements.Resize(size);

        for (SizeType i = 0; i < size; i++)
        {
            if (fbom::FBOMResult err = HypDataHelper<T>::Serialize(value[i], elements[i]))
            {
                return err;
            }
        }

        out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(elements[0].GetType(), std::move(elements)));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        fbom::FBOMArray array { fbom::FBOMUnset() };

        if (fbom::FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        Array<T, AllocatorType> result;
        result.Reserve(size);

        for (SizeType i = 0; i < size; i++)
        {
            HypData element;

            if (fbom::FBOMResult err = HypDataHelper<T>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result.PushBack(std::move(element.Get<T>()));
        }

        HypDataHelper<Array<T, AllocatorType>> {}.Set(out, std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class T, SizeType Size>
struct HypDataHelperDecl<FixedArray<T, Size>>
{
};

template <class T, SizeType Size>
struct HypDataHelper<FixedArray<T, Size>, std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE FixedArray<T, Size>& Get(const Any& value) const
    {
        return value.Get<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const FixedArray<T, Size>& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<FixedArray<T, Size>>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, FixedArray<T, Size>&& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<FixedArray<T, Size>>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(const FixedArray<T, Size>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        if (Size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(detail::HypDataPlaceholderSerializedType<T>::Get()));

            return fbom::FBOMResult::FBOM_OK;
        }

        Array<fbom::FBOMData> elements;
        elements.Resize(Size);

        for (SizeType i = 0; i < Size; i++)
        {
            if (fbom::FBOMResult err = HypDataHelper<T>::Serialize(value[i], elements[i]))
            {
                return err;
            }
        }

        out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(elements[0].GetType(), std::move(elements)));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        fbom::FBOMArray array { fbom::FBOMUnset() };

        if (fbom::FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        if (Size != array.Size())
        {
            return { fbom::FBOMResult::FBOM_ERR, "Failed to deserialize array - size does not match expected size" };
        }

        FixedArray<T, Size> result;

        for (SizeType i = 0; i < Size; i++)
        {
            HypData element;

            if (fbom::FBOMResult err = HypDataHelper<T>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result[i] = std::move(element.Get<T>());
        }

        HypDataHelper<FixedArray<T, Size>> {}.Set(out, std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class T, SizeType Size>
struct HypDataHelperDecl<T[Size]>
{
};

template <class T, SizeType Size>
struct HypDataHelper<T[Size], std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<FixedArray<T, Size>>
{
    using ConvertibleFrom = Tuple<FixedArray<T, Size>>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE FixedArray<T, Size>& Get(const Any& value) const
    {
        return value.Get<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const T (&value)[Size]) const
    {
        HypDataHelper<FixedArray<T, Size>>::Set(hyp_data, MakeFixedArray(value));
    }

    static fbom::FBOMResult Serialize(const T (&value)[Size], fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        if (Size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(detail::HypDataPlaceholderSerializedType<T>::Get()));

            return fbom::FBOMResult::FBOM_OK;
        }

        Array<fbom::FBOMData> elements;
        elements.Resize(Size);

        for (SizeType i = 0; i < Size; i++)
        {
            if (fbom::FBOMResult err = HypDataHelper<T>::Serialize(value[i], elements[i]))
            {
                return err;
            }
        }

        out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(elements[0].GetType(), std::move(elements)));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        fbom::FBOMArray array { fbom::FBOMUnset() };

        if (fbom::FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        if (Size != array.Size())
        {
            return { fbom::FBOMResult::FBOM_ERR, "Failed to deserialize array - size does not match expected size" };
        }

        FixedArray<T, Size> result;

        for (SizeType i = 0; i < Size; i++)
        {
            HypData element;

            if (fbom::FBOMResult err = HypDataHelper<T>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result[i] = std::move(element.Get<T>());
        }

        HypDataHelper<FixedArray<T, Size>> {}.Set(out, std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class K, class V>
struct HypDataHelperDecl<Pair<K, V>>
{
};

template <class K, class V>
struct HypDataHelper<Pair<K, V>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Pair<K, V>>();
    }

    HYP_FORCE_INLINE Pair<K, V>& Get(const Any& value) const
    {
        return value.Get<Pair<K, V>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const Pair<K, V>& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Pair<K, V>>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, Pair<K, V>&& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Pair<K, V>>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(const Pair<K, V>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        fbom::FBOMData first_data;
        fbom::FBOMData second_data;

        if (fbom::FBOMResult err = HypDataHelper<K>::Serialize(value.first, first_data))
        {
            return err;
        }

        if (fbom::FBOMResult err = HypDataHelper<V>::Serialize(value.second, second_data))
        {
            return err;
        }

        fbom::FBOMObject object(fbom::FBOMObjectType(TypeWrapper<Pair<K, V>> {}, FBOMTypeFlags::DEFAULT, fbom::FBOMBaseObjectType()));
        object.SetProperty("Key", std::move(first_data));
        object.SetProperty("Value", std::move(second_data));

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        HypData first;
        HypData second;

        if (fbom::FBOMResult err = HypDataHelper<K>::Deserialize(context, object.GetProperty("Key"), first))
        {
            return err;
        }

        if (fbom::FBOMResult err = HypDataHelper<V>::Deserialize(context, object.GetProperty("Value"), second))
        {
            return err;
        }

        HypDataHelper<Pair<K, V>> {}.Set(out, Pair<K, V> { first.Get<K>(), second.Get<V>() });

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class K, class V>
struct HypDataHelperDecl<HashMap<K, V>>
{
};

template <class K, class V>
struct HypDataHelper<HashMap<K, V>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<HashMap<K, V>>();
    }

    HYP_FORCE_INLINE HashMap<K, V>& Get(const Any& value) const
    {
        return value.Get<HashMap<K, V>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const HashMap<K, V>& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<HashMap<K, V>>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, HashMap<K, V>&& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<HashMap<K, V>>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(const HashMap<K, V>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(detail::HypDataPlaceholderSerializedType<Pair<K, V>>::Get()));

            return fbom::FBOMResult::FBOM_OK;
        }

        Array<fbom::FBOMData> elements;
        elements.Reserve(size);

        uint32 element_index = 0;

        for (const Pair<K, V>& pair : value)
        {
            fbom::FBOMData& element = elements.EmplaceBack();

            if (fbom::FBOMResult err = HypDataHelper<Pair<K, V>>::Serialize(pair, element))
            {
                return err;
            }
        }

        out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(elements[0].GetType(), std::move(elements)));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        fbom::FBOMArray array { fbom::FBOMUnset() };

        if (fbom::FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        HashMap<K, V> result;

        for (SizeType i = 0; i < size; i++)
        {
            HypData element;

            if (fbom::FBOMResult err = HypDataHelper<Pair<K, V>>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result.Insert(std::move(element.Get<Pair<K, V>>()));
        }

        HypDataHelper<HashMap<K, V>> {}.Set(out, std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class ValueType, auto KeyByFunction>
struct HypDataHelperDecl<HashSet<ValueType, KeyByFunction>>
{
};

template <class ValueType, auto KeyByFunction>
struct HypDataHelper<HashSet<ValueType, KeyByFunction>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<HashSet<ValueType, KeyByFunction>>();
    }

    HYP_FORCE_INLINE HashSet<ValueType, KeyByFunction>& Get(const Any& value) const
    {
        return value.Get<HashSet<ValueType, KeyByFunction>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const HashSet<ValueType, KeyByFunction>& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<HashSet<ValueType, KeyByFunction>>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, HashSet<ValueType, KeyByFunction>&& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<HashSet<ValueType, KeyByFunction>>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(const HashSet<ValueType, KeyByFunction>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(detail::HypDataPlaceholderSerializedType<ValueType>::Get()));

            return fbom::FBOMResult::FBOM_OK;
        }

        Array<fbom::FBOMData> elements;
        elements.Reserve(size);

        uint32 element_index = 0;

        for (const ValueType& value : value)
        {
            fbom::FBOMData& element = elements.EmplaceBack();

            if (fbom::FBOMResult err = HypDataHelper<ValueType>::Serialize(value, element))
            {
                return err;
            }
        }

        out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(elements[0].GetType(), std::move(elements)));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        fbom::FBOMArray array { fbom::FBOMUnset() };

        if (fbom::FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        HashSet<ValueType, KeyByFunction> result;

        for (SizeType i = 0; i < size; i++)
        {
            HypData element;

            if (fbom::FBOMResult err = HypDataHelper<ValueType>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result.Insert(std::move(element.Get<ValueType>()));
        }

        HypDataHelper<HashSet<ValueType, KeyByFunction>> {}.Set(out, std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

// fwd decl for math types
namespace math {
namespace detail {

template <class T>
struct Vec2;

template <class T>
struct Vec3;

template <class T>
struct Vec4;

} // namespace detail
} // namespace math

template <class T>
struct HypDataHelperDecl<math::detail::Vec2<T>>
{
};

template <class T>
struct HypDataHelper<math::detail::Vec2<T>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<math::detail::Vec2<T>>();
    }

    HYP_FORCE_INLINE math::detail::Vec2<T>& Get(Any& value) const
    {
        return value.Get<math::detail::Vec2<T>>();
    }

    HYP_FORCE_INLINE const math::detail::Vec2<T>& Get(const Any& value) const
    {
        return value.Get<math::detail::Vec2<T>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const math::detail::Vec2<T>& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<math::detail::Vec2<T>>(value));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(const math::detail::Vec2<T>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        out_data = fbom::FBOMData(value);

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        math::detail::Vec2<T> result;

        if (fbom::FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class T>
struct HypDataHelperDecl<math::detail::Vec3<T>>
{
};

template <class T>
struct HypDataHelper<math::detail::Vec3<T>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<math::detail::Vec3<T>>();
    }

    HYP_FORCE_INLINE math::detail::Vec3<T>& Get(Any& value) const
    {
        return value.Get<math::detail::Vec3<T>>();
    }

    HYP_FORCE_INLINE const math::detail::Vec3<T>& Get(const Any& value) const
    {
        return value.Get<math::detail::Vec3<T>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const math::detail::Vec3<T>& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<math::detail::Vec3<T>>(value));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(const math::detail::Vec3<T>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        out_data = fbom::FBOMData(value);

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        math::detail::Vec3<T> result;

        if (fbom::FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class T>
struct HypDataHelperDecl<math::detail::Vec4<T>>
{
};

template <class T>
struct HypDataHelper<math::detail::Vec4<T>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<math::detail::Vec4<T>>();
    }

    HYP_FORCE_INLINE math::detail::Vec4<T>& Get(Any& value) const
    {
        return value.Get<math::detail::Vec4<T>>();
    }

    HYP_FORCE_INLINE const math::detail::Vec4<T>& Get(const Any& value) const
    {
        return value.Get<math::detail::Vec4<T>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const math::detail::Vec4<T>& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<math::detail::Vec4<T>>(value));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(const math::detail::Vec4<T>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        out_data = fbom::FBOMData(value);

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        math::detail::Vec4<T> result;

        if (fbom::FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Matrix3>
{
};

template <>
struct HypDataHelper<Matrix3> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Matrix3>();
    }

    HYP_FORCE_INLINE Matrix3& Get(Any& value) const
    {
        return value.Get<Matrix3>();
    }

    HYP_FORCE_INLINE const Matrix3& Get(const Any& value) const
    {
        return value.Get<Matrix3>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const Matrix3& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Matrix3>(value));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(const Matrix3& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        out_data = fbom::FBOMData(value);

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        Matrix3 result;

        if (fbom::FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Matrix4>
{
};

template <>
struct HypDataHelper<Matrix4> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Matrix4>();
    }

    HYP_FORCE_INLINE Matrix4& Get(Any& value) const
    {
        return value.Get<Matrix4>();
    }

    HYP_FORCE_INLINE const Matrix4& Get(const Any& value) const
    {
        return value.Get<Matrix4>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const Matrix4& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Matrix4>(value));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(const Matrix4& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        out_data = fbom::FBOMData(value);

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        Matrix4 result;

        if (fbom::FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Quaternion>
{
};

template <>
struct HypDataHelper<Quaternion> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Quaternion>();
    }

    HYP_FORCE_INLINE Quaternion& Get(Any& value) const
    {
        return value.Get<Quaternion>();
    }

    HYP_FORCE_INLINE const Quaternion& Get(const Any& value) const
    {
        return value.Get<Quaternion>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const Quaternion& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Quaternion>(value));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(const Quaternion& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        out_data = fbom::FBOMData(value);

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        Quaternion result;

        if (fbom::FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<ByteBuffer>
{
};

template <>
struct HypDataHelper<ByteBuffer> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<ByteBuffer>();
    }

    HYP_FORCE_INLINE ByteBuffer& Get(const Any& value) const
    {
        return value.Get<ByteBuffer>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const ByteBuffer& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<ByteBuffer>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, ByteBuffer&& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<ByteBuffer>(std::move(value)));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(const ByteBuffer& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        out_data = fbom::FBOMData::FromByteBuffer(value);

        return fbom::FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        ByteBuffer byte_buffer;

        if (fbom::FBOMResult err = data.ReadByteBuffer(byte_buffer))
        {
            return err;
        }

        out = HypData(std::move(byte_buffer));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class... Types>
struct HypDataHelperDecl<Variant<Types...>>
{
};

template <class... Types>
struct HypDataHelper<Variant<Types...>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<Types...>;

    template <class T>
    static fbom::FBOMResult VariantElementSerializeHelper(const Variant<Types...>& variant, fbom::FBOMData& out_data)
    {
        return HypDataHelper<T>::Serialize(variant.template Get<T>(), out_data);
    }

    template <class T>
    static fbom::FBOMResult VariantElementDeserializeHelper(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HypData tmp;
        if (fbom::FBOMResult err = HypDataHelper<T>::Deserialize(context, data, tmp))
        {
            return err;
        }

        out = HypData(Variant<Types...>(std::move(tmp).template Get<T>()));

        return fbom::FBOMResult::FBOM_OK;
    }

    static constexpr std::add_pointer_t<fbom::FBOMResult(const Variant<Types...>&, fbom::FBOMData&)> element_serialize_functions[] = { &VariantElementSerializeHelper<Types>... };
    static constexpr std::add_pointer_t<fbom::FBOMResult(fbom::FBOMLoadContext&, const fbom::FBOMData&, HypData&)> element_deserialize_functions[] = { &VariantElementDeserializeHelper<Types>... };

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Variant<Types...>>();
    }

    HYP_FORCE_INLINE Variant<Types...>& Get(Any& value) const
    {
        return value.Get<Variant<Types...>>();
    }

    HYP_FORCE_INLINE const Variant<Types...>& Get(const Any& value) const
    {
        return value.Get<Variant<Types...>>();
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, Variant<Types...>> && std::disjunction_v<std::is_same<NormalizedType<T>, Types>...>>>
    HYP_FORCE_INLINE constexpr bool Is(const T& value) const
    {
        return true;
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, Variant<Types...>> && std::disjunction_v<std::is_same<NormalizedType<T>, Types>...>>>
    HYP_FORCE_INLINE Variant<Types...> Get(const T& value) const
    {
        return Variant<Types...>(value);
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, Variant<Types...>> && std::disjunction_v<std::is_same<NormalizedType<T>, Types>...>>>
    HYP_FORCE_INLINE Variant<Types...> Get(T&& value) const
    {
        return Variant<Types...>(std::forward<T>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const Variant<Types...>& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Variant<Types...>>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, Variant<Types...>&& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Variant<Types...>>(std::move(value)));
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Serialize(const Variant<Types...>& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        const int type_index = value.GetTypeIndex();

        if (type_index == Variant<Types...>::invalid_type_index)
        {
            out_data = fbom::FBOMData();

            return fbom::FBOMResult::FBOM_OK;
        }

        return element_serialize_functions[type_index](value, out_data);
    }

    HYP_FORCE_INLINE static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        if (data.IsUnset())
        {
            out = HypData(Variant<Types...>());

            return fbom::FBOMResult::FBOM_OK;
        }

        int found_type_index = Variant<Types...>::invalid_type_index;
        int current_type_index = 0;

        for (TypeID type_id : Variant<Types...>::type_ids)
        {
            if (data.GetType().GetNativeTypeID() == type_id)
            {
                found_type_index = current_type_index;

                break;
            }

            ++current_type_index;
        }

        if (found_type_index == Variant<Types...>::invalid_type_index)
        {
            return { fbom::FBOMResult::FBOM_ERR, "Cannot deserialize variant - type not found" };
        }

        return element_deserialize_functions[found_type_index](context, data, out);
    }
};

template <class T>
struct HypDataHelper<T, std::enable_if_t<!HypData::can_store_directly<T> && !implementation_exists<HypDataHelperDecl<T>>>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<T*, AnyRef, AnyHandle, RC<void>>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(T* value) const
    {
        // Dereferencing a null pointer would be bad - so we'll just pretend it's not the type
        return value != nullptr;
    }

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        if constexpr (has_handle_definition<T>)
        {
            return value.Is<T>();
        }
        else
        {
            return false;
        }
    }

    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE T& Get(const Any& value) const
    {
        return value.Get<T>();
    }

    HYP_FORCE_INLINE T& Get(T* value) const
    {
        return *value;
    }

    HYP_FORCE_INLINE T& Get(const AnyRef& value) const
    {
        return value.Get<T>();
    }

    HYP_FORCE_INLINE T& Get(const AnyHandle& value) const
    {
        if constexpr (has_handle_definition<T>)
        {
            return *value.Cast<T>().Get();
        }
        else
        {
            HYP_UNREACHABLE();
        }
    }

    HYP_FORCE_INLINE T& Get(const RC<void>& value) const
    {
        return *value.CastUnsafe_Internal<T>();
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, const T& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<T>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hyp_data, T&& value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<T>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(const T& value, fbom::FBOMData& out_data)
    {
        HYP_SCOPE;

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<NormalizedType<T>>());

        if (!marshal)
        {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(ConstAnyRef(value), object))
        {
            return err;
        }

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(fbom::FBOMLoadContext& context, const fbom::FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        const fbom::FBOMMarshalerBase* marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal)
        {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data)
        {
            out = HypData(T {});

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return fbom::FBOMResult::FBOM_OK;
    }
};

namespace detail {

#pragma region HypDataTypeChecker implementation

template <class T>
struct HypDataTypeChecker
{
    HYP_FORCE_INLINE bool operator()(const HypData::VariantType& value) const
    {
        constexpr bool should_skip_additional_is_check = std::is_same_v<T, typename HypDataHelper<T>::StorageType>;

        static_assert(HypData::can_store_directly<typename HypDataHelper<T>::StorageType>, "StorageType must be a type that can be stored directly in the HypData variant without allocating memory dynamically");

        return value.Is<typename HypDataHelper<T>::StorageType>()
            && (should_skip_additional_is_check || HypDataHelper<T> {}.Is(value.Get<typename HypDataHelper<T>::StorageType>()));
    }
};

template <class T, class... ConvertibleFrom>
struct HypDataTypeChecker_Tuple<T, Tuple<ConvertibleFrom...>>
{
    HYP_FORCE_INLINE bool operator()(const HypData::VariantType& value) const
    {
        return HypDataTypeChecker<T> {}(value) || (HypDataTypeChecker<ConvertibleFrom> {}(value) || ...);
    }
};

#pragma endregion HypDataTypeChecker implementation

#pragma region Helpers

template <class T>
struct HypDataPlaceholderSerializedType
{
    static inline fbom::FBOMType Get()
    {
        thread_local bool is_init = false;
        thread_local fbom::FBOMType placeholder_type = fbom::FBOMPlaceholderType();

        if (!is_init)
        {
            is_init = true;

            fbom::FBOMData placeholder_data;

            if (fbom::FBOMResult err = HypDataHelper<T>::Serialize(T {}, placeholder_data))
            {
                HYP_FAIL("Failed to serialize placeholder data for type %s: %s", TypeName<T>().Data(), *err.message);
            }

            placeholder_type = placeholder_data.GetType();
        }

        return placeholder_type;
    }
};

#pragma endregion Helpers

#pragma region HypDataGetter implementation

// template <class ReturnType, class T>
// struct HypDataGetter
// {
//     HYP_FORCE_INLINE bool operator()(HypData::VariantType &value, ValueStorage<ReturnType> &out_storage) const
//     {
//         if constexpr (HypData::can_store_directly<typename HypDataHelper<T>::StorageType>) {
//             return HypDataHelper<NormalizedType<T>>{}.Get(value.Get<typename HypDataHelper<T>::StorageType>(), out_storage);
//         } else {
//             return HypDataHelper<NormalizedType<T>>{}.Get(value.Get<Any>().Get<typename HypDataHelper<T>::StorageType>(), out_storage);
//         }
//     }

//     HYP_FORCE_INLINE bool operator()(const HypData::VariantType &value, ValueStorage<const ReturnType> &out_storage) const
//     {
//         if constexpr (HypData::can_store_directly<typename HypDataHelper<T>::StorageType>) {
//             return HypDataHelper<NormalizedType<T>>{}.Get(value.Get<typename HypDataHelper<T>::StorageType>(), out_storage);
//         } else {
//             return HypDataHelper<NormalizedType<T>>{}.Get(value.Get<Any>().Get<typename HypDataHelper<T>::StorageType>(), out_storage);
//         }
//     }
// };

template <class VariantType, class ReturnType, class... Types, SizeType... Indices>
HYP_FORCE_INLINE bool HypDataGetter_Tuple_Impl(VariantType&& value, Optional<ReturnType>& out_value, std::index_sequence<Indices...>)
{
    const auto get_for_type_index = [&value]<SizeType SelectedTypeIndex>(Optional<ReturnType>& out_value, std::integral_constant<SizeType, SelectedTypeIndex>) -> bool
    {
        using SelectedType = typename TupleElement<SelectedTypeIndex, Types...>::Type;
        using StorageType = typename HypDataHelper<SelectedType>::StorageType;

        static_assert(HypData::can_store_directly<typename HypDataHelper<NormalizedType<ReturnType>>::StorageType>);

        if constexpr (std::is_same_v<NormalizedType<ReturnType>, StorageType>)
        {
            out_value.Set(value.template Get<StorageType>());
        }
        else
        {
            decltype(auto) internal_value = value.template Get<StorageType>();

            if (!HypDataHelper<NormalizedType<ReturnType>> {}.Is(internal_value))
            {
                return false;
            }

            out_value.Set(HypDataHelper<NormalizedType<ReturnType>> {}.Get(std::forward<decltype(internal_value)>(internal_value)));
        }

        return true;
    };

    return ((HypDataTypeChecker<Types> {}(value) && get_for_type_index(out_value, std::integral_constant<SizeType, Indices> {})) || ...);
}

template <class ReturnType, class T, class... ConvertibleFrom>
struct HypDataGetter_Tuple<ReturnType, T, Tuple<ConvertibleFrom...>>
{
    HYP_FORCE_INLINE bool operator()(HypData::VariantType& value, Optional<ReturnType>& out_value) const
    {
        return HypDataGetter_Tuple_Impl<HypData::VariantType&, ReturnType, T, ConvertibleFrom...>(value, out_value, std::index_sequence_for<T, ConvertibleFrom...> {});
    }

    HYP_FORCE_INLINE bool operator()(const HypData::VariantType& value, Optional<ReturnType>& out_value) const
    {
        return HypDataGetter_Tuple_Impl<const HypData::VariantType&, ReturnType, T, ConvertibleFrom...>(value, out_value, std::index_sequence_for<T, ConvertibleFrom...> {});
    }
};

#pragma endregion HypDataGetter implementation

} // namespace detail

static_assert(sizeof(HypData) == 40, "sizeof(HypData) must match C# struct size");

template <class T>
constexpr bool is_hypdata_v = std::is_same_v<NormalizedType<T>, HypData>;

} // namespace hyperion

#endif