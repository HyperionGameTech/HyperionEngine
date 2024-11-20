/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_DATA_HPP
#define HYPERION_CORE_HYP_DATA_HPP

#include <core/Defines.hpp>

#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/ByteBuffer.hpp>

#include <asset/serialization/fbom/FBOMResult.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/FBOMArray.hpp>
#include <asset/serialization/fbom/FBOM.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

struct HypData;

class Node;
class NodeProxy;

class Entity;

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

using HypDataSerializeFunction = fbom::FBOMResult(*)(HypData &&hyp_data, fbom::FBOMData &out_data);

/*! \brief A type-safe union that can store any type of data, abstracting away internal engine structures such as Handle<T>, RC<T>, etc.
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
        IDBase,
        AnyHandle,
        RC<void>,
        AnyRef,
        Any
    >;

    template <class T>
    static constexpr bool can_store_directly =
        /* Fundamental types - can be stored inline */
        std::is_same_v<T, int8> || std::is_same_v<T, int16> | std::is_same_v<T, int32> | std::is_same_v<T, int64>
        || std::is_same_v<T, uint8> || std::is_same_v<T, uint16> || std::is_same_v<T, uint32> || std::is_same_v<T, uint64>
        || std::is_same_v<T, char>
        || std::is_same_v<T, float> || std::is_same_v<T, double>
        || std::is_same_v<T, bool>
        
        /*! All ID<T> are stored as IDBase */
        || std::is_base_of_v<IDBase, T>

        /*! Handle<T> gets stored as AnyHandle, which holds TypeID for conversion */
        || std::is_base_of_v<HandleBase, T> || std::is_same_v<T, AnyHandle>

        /*! RC<T> gets stored as RC<void> and can be converted back */
        || std::is_base_of_v<typename RC<void>::RefCountedPtrBase, T>

        /*! Pointers are stored as AnyRef which holds TypeID for conversion */
        || std::is_same_v<T, AnyRef> || std::is_pointer_v<T>
        
        || std::is_same_v<T, Any>;

    VariantType                 value;

    HypData()
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< T, HypData > > >
    explicit HypData(T &&value)
        : HypData()
    {
        HYP_NAMED_SCOPE("Construct HypData from T");

        HypDataHelper<NormalizedType<T>>{}.Set(*this, std::forward<T>(value));
    }
    
    template <class T, typename = std::enable_if_t< !std::is_same_v< T, HypData > > >
    HypData &operator=(T &&value)
    {
        HYP_NAMED_SCOPE("Assign HypData from T");

        HypDataHelper<NormalizedType<T>>{}.Set(*this, std::forward<T>(value));

        return *this;
    }

    HypData(const HypData &other)                   = delete;
    HypData &operator=(const HypData &other)        = delete;

    HypData(HypData &&other) noexcept               = default;
    HypData &operator=(HypData &&other) noexcept    = default;

    ~HypData()                                      = default;

    HYP_FORCE_INLINE bool IsNull() const
        { return !ToRef().HasValue(); }

    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return ToRef().GetTypeID(); }

    HYP_FORCE_INLINE AnyRef ToRef()
    {
        HYP_SCOPE;

        if (!value.IsValid()) {
            return AnyRef();
        }

        if (AnyHandle *any_handle_ptr = value.TryGet<AnyHandle>()) {
            return any_handle_ptr->ToRef();
        }

        if (RC<void> *rc_ptr = value.TryGet<RC<void>>()) {
            return rc_ptr->ToRef();
        }

        if (AnyRef *any_ref_ptr = value.TryGet<AnyRef>()) {
            return *any_ref_ptr;
        }

        if (Any *any_ptr = value.TryGet<Any>()) {
            return any_ptr->ToRef();
        }

        return AnyRef(value.GetTypeID(), value.GetPointer());
    }

    HYP_FORCE_INLINE ConstAnyRef ToRef() const
        { return const_cast<HypData *>(this)->ToRef(); }

    template <class T>
    HYP_FORCE_INLINE bool Is(bool strict = false) const
    {
        HYP_SCOPE;

        static_assert(!std::is_same_v<T, HypData>);

        if (!value.IsValid()) {
            return false;
        }
        
        if (strict) {
            return detail::HypDataTypeChecker_Tuple<T, Tuple<>>{}(value);
        }
        
        return detail::HypDataTypeChecker_Tuple<T, typename HypDataHelper<T>::ConvertibleFrom>{}(value);
    }

    template <class T>
    HYP_FORCE_INLINE auto Get() -> typename detail::HypDataGetReturnTypeHelper<T, false>::Type
    {
        static_assert(!std::is_same_v<T, HypData>);

        HYP_SCOPE;

        using ReturnType = typename detail::HypDataGetReturnTypeHelper<T, false>::Type;

        Optional<ReturnType> result_value;

        detail::HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getter_instance { };

        AssertThrowMsg(getter_instance(value, result_value),
            "Failed to invoke HypData Get method with T = %s - Mismatched types or T could not be converted to the held type (current TypeID = %u)",
            TypeName<T>().Data(),
            GetTypeID().Value());

        return *result_value;
    }

    template <class T>
    HYP_FORCE_INLINE auto Get() const -> typename detail::HypDataGetReturnTypeHelper<T, true>::Type
    {
        static_assert(!std::is_same_v<T, HypData>);

        HYP_SCOPE;

        using ReturnType = typename detail::HypDataGetReturnTypeHelper<T, true>::Type;

        Optional<ReturnType> result_value;

        detail::HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getter_instance { };

        AssertThrowMsg(getter_instance(value, result_value),
            "Failed to invoke HypData Get method with T = %s - Mismatched types or T could not be converted to the held type (current TypeID = %u)",
            TypeName<T>().Data(),
            GetTypeID().Value());

        return *result_value;
    }

    template <class T>
    HYP_FORCE_INLINE auto TryGet() -> Optional<typename detail::HypDataGetReturnTypeHelper<T, false>::Type>
    {
        static_assert(!std::is_same_v<T, HypData>);

        HYP_SCOPE;

        using ReturnType = typename detail::HypDataGetReturnTypeHelper<T, false>::Type;

        Optional<ReturnType> result_value;

        detail::HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getter_instance { };
        getter_instance(value, result_value);

        return result_value;
    }

    template <class T>
    HYP_FORCE_INLINE auto TryGet() const -> Optional<typename detail::HypDataGetReturnTypeHelper<T, true>::Type>
    {
        static_assert(!std::is_same_v<T, HypData>);

        HYP_SCOPE;

        using ReturnType = typename detail::HypDataGetReturnTypeHelper<T, true>::Type;

        Optional<ReturnType> result_value;

        detail::HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getter_instance { };
        getter_instance(value, result_value);

        return result_value;
    }
};

#pragma region HypDataMarshalHelper

struct HypDataMarshalHelper
{
    static HYP_API fbom::FBOMResult NoMarshalRegistered(ANSIStringView type_name);

    template <class T>
    static inline fbom::FBOMResult Serialize(const T &value, fbom::FBOMData &out_data)
    {
        using Normalized = NormalizedType<T>;

        HYP_SCOPE;

        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<Normalized>());

        if (!marshal) {
            return NoMarshalRegistered(TypeName<Normalized>());
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(ConstAnyRef(value), object)) {
            return err;
        }

        object.m_deserialized_object.Emplace(value);

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }

    template <class T>
    static inline fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal) {
            return NoMarshalRegistered(TypeName<T>());
        }

        if (!data) {
            return { fbom::FBOMResult::FBOM_ERR, "Cannot deserialize unset value" };
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(object)) {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(object, out)) {
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
struct HypDataHelperDecl<T, std::enable_if_t<std::is_fundamental_v<T>>> {};

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
        bool
    >;

    HYP_FORCE_INLINE bool Is(T value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    template <class OtherT, typename = std::enable_if_t< !std::is_same_v<OtherT, T> > >
    HYP_FORCE_INLINE bool Is(OtherT value) const
    {
        return std::is_fundamental_v<OtherT>;
    }

    constexpr T Get(T value) const
    {
        return value;
    }

    template <class OtherT, typename = std::enable_if_t< !std::is_same_v<OtherT, T> > >
    constexpr T Get(OtherT value) const
    {
        return static_cast<T>(value);
    }

    void Set(HypData &hyp_data, T value) const
    {
        hyp_data.value.Set<T>(value);
    }

    static fbom::FBOMResult Serialize(const HypData &data, fbom::FBOMData &out)
    {
        out = fbom::FBOMData(data.value.Get<T>());

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        T value;

        if (fbom::FBOMResult err = data.Read(&value)) {
            return err;
        }

        out = HypData(value);

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<T, std::enable_if_t<std::is_enum_v<T>>> {};

template <class T>
struct HypDataHelper<T, std::enable_if_t<std::is_enum_v<T>>> : HypDataHelper<std::underlying_type_t<T>>
{
    using ConvertibleFrom = Tuple<std::underlying_type_t<T>>;

    HYP_FORCE_INLINE bool Is(const std::underlying_type_t<T> &value) const
    {
        return true;
    }

    constexpr T Get(T value) const
    {
        return value;
    }

    constexpr T Get(std::underlying_type_t<T> value) const
    {
        return static_cast<T>(value);
    }

    void Set(HypData &hyp_data, T value) const
    {
        HypDataHelper<std::underlying_type_t<T>>::Set(hyp_data, static_cast<std::underlying_type_t<T>>(value));
    }
};

template <class T>
struct HypDataHelperDecl<EnumFlags<T>> {};

template <class T>
struct HypDataHelper<EnumFlags<T>> : HypDataHelper<typename EnumFlags<T>::UnderlyingType>
{
    using ConvertibleFrom = Tuple<typename EnumFlags<T>::UnderlyingType>;

    HYP_FORCE_INLINE bool Is(typename EnumFlags<T>::UnderlyingType value) const
    {
        return true;
    }

    constexpr EnumFlags<T> Get(EnumFlags<T> value) const
    {
        return value;
    }

    constexpr EnumFlags<T> Get(typename EnumFlags<T>::UnderlyingType value) const
    {
        return static_cast<EnumFlags<T>>(value);
    }

    void Set(HypData &hyp_data, EnumFlags<T> value) const
    {
        HypDataHelper<typename EnumFlags<T>::UnderlyingType>::Set(hyp_data, static_cast<typename EnumFlags<T>::UnderlyingType>(value));
    }
};

template <>
struct HypDataHelperDecl<IDBase> {};

template <>
struct HypDataHelper<IDBase>
{
    using StorageType = IDBase;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const IDBase &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    constexpr IDBase &Get(IDBase &value) const
    {
        return value;
    }

    const IDBase &Get(const IDBase &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, const IDBase &value) const
    {
        hyp_data.value.Set<IDBase>(value);
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        out_data = fbom::FBOMData::FromStruct<IDBase>(hyp_data.Get<IDBase>());

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        IDBase value;
        
        if (fbom::FBOMResult err = data.ReadStruct<IDBase>(&value)) {
            return err;
        }

        out = HypData(value);

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<ID<T>> {};

template <class T>
struct HypDataHelper<ID<T>> : HypDataHelper<IDBase>
{
    using ConvertibleFrom = Tuple<AnyHandle>;

    HYP_FORCE_INLINE bool Is(const IDBase &value) const
    {
        return true; // can't do anything more to check as IDBase doesn't hold type info.
    }

    HYP_FORCE_INLINE bool Is(const AnyHandle &value) const
    {
        return value.GetTypeID() == TypeID::ForType<T>();
    }

    ID<T> Get(IDBase value) const
    {
        return ID<T>(value);
    }

    ID<T> Get(const AnyHandle &value) const
    {
        return ID<T>(value.GetID());
    }

    void Set(HypData &hyp_data, const ID<T> &value) const
    {
        HypDataHelper<IDBase>::Set(hyp_data, static_cast<const IDBase &>(value));
    }
};

// template <>
// struct HypDataHelperDecl<ID<Entity>> {};

// template <>
// struct HypDataHelper<ID<Entity>> : HypDataHelper<IDBase>
// {
//     using ConvertibleFrom = Tuple<>;

//     HYP_FORCE_INLINE bool Is(const IDBase &value) const
//     {
//         return true; // can't do anything more to check as IDBase doesn't hold type info.
//     }

//     ID<Entity> &Get(IDBase &value) const
//     {
//         return static_cast<ID<Entity> &>(value);
//     }

//     const ID<Entity> &Get(const IDBase &value) const
//     {
//         return static_cast<const ID<Entity> &>(value);
//     }

//     void Set(HypData &hyp_data, const ID<Entity> &value) const
//     {
//         HypDataHelper<IDBase>::Set(hyp_data, static_cast<const IDBase &>(value));
//     }

//     static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
//     {
//         ID<Entity> entity = static_cast<ID<Entity>>(hyp_data.Get<IDBase>());
        
//         return HypDataMarshalHelper::Serialize<ID<Entity>>(entity, out_data);
//     }

//     static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
//     {
//         return HypDataMarshalHelper::Deserialize<ID<Entity>>(data, out);
//     }
// };

template <>
struct HypDataHelperDecl<AnyHandle> {};

template <>
struct HypDataHelper<AnyHandle>
{
    using StorageType = AnyHandle;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyHandle &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    AnyHandle &Get(AnyHandle &value) const
    {
        return value;
    }

    const AnyHandle &Get(const AnyHandle &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, const AnyHandle &value) const
    {
        hyp_data.value.Set<AnyHandle>(value);
    }

    void Set(HypData &hyp_data, AnyHandle &&value) const
    {
        hyp_data.value.Set<AnyHandle>(std::move(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;

        const AnyHandle &value = hyp_data.Get<AnyHandle>();

        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(value.GetTypeID());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
        }

        if (!value.IsValid()) {
            // unset
            out_data = fbom::FBOMData();

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(value.ToRef(), object)) {
            return err;
        }

        object.m_deserialized_object.Emplace(std::move(value));

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<Handle<T>> {};

template <class T>
struct HypDataHelper<Handle<T>> : HypDataHelper<AnyHandle>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyHandle &value) const
    {
        return value.type_id == TypeID::ForType<T>();
    }

    Handle<T> Get(const AnyHandle &value) const
    {
        return value.Cast<T>();
    }

    void Set(HypData &hyp_data, const Handle<T> &value) const
    {
        HypDataHelper<AnyHandle>::Set(hyp_data, AnyHandle(value));
    }

    void Set(HypData &hyp_data, Handle<T> &&value) const
    {
        HypDataHelper<AnyHandle>::Set(hyp_data, AnyHandle(std::move(value)));
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;

        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
        }

        if (!data) {
            out = HypData(Handle<T> { });

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(object)) {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(object, out)) {
            return err;
        }

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <>
struct HypDataHelperDecl<RC<void>> {};

template <>
struct HypDataHelper<RC<void>>
{
    using StorageType = RC<void>;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const RC<void> &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    RC<void> &Get(RC<void> &value) const
    {
        return value;
    }

    const RC<void> &Get(const RC<void> &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, const RC<void> &value) const
    {
        hyp_data.value.Set<RC<void>>(value);
    }

    void Set(HypData &hyp_data, RC<void> &&value) const
    {
        hyp_data.value.Set<RC<void>>(std::move(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;

        const RC<void> &value = hyp_data.Get<RC<void>>();

        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(value.GetTypeID());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        if (!value) {
            // unset
            out_data = fbom::FBOMData();

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(value.ToRef(), object)) {
            return err;
        }

        object.m_deserialized_object.Emplace(std::move(value));

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<RC<T>, std::enable_if_t< !std::is_void_v<T> >> { };

template <class T>
struct HypDataHelper<RC<T>, std::enable_if_t< !std::is_void_v<T> >> : HypDataHelper<RC<void>>
{
    HYP_FORCE_INLINE bool Is(const RC<void> &value) const
    {
        return value.Is<T>();
    }

    RC<T> Get(const RC<void> &value) const
    {
        return value.template Cast<T>();
    }

    void Set(HypData &hyp_data, const RC<T> &value) const
    {
        HypDataHelper<RC<void>>::Set(hyp_data, value);
    }

    void Set(HypData &hyp_data, RC<T> &&value) const
    {
        HypDataHelper<RC<void>>::Set(hyp_data, std::move(value));
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data) {
            out = HypData(RC<T> { });

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(object)) {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(object, out)) {
            return err;
        }

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <>
struct HypDataHelperDecl<NodeProxy> { };

template <>
struct HypDataHelper<NodeProxy> : HypDataHelper<RC<Node>>
{
};

template <>
struct HypDataHelperDecl<AnyRef> { };

template <>
struct HypDataHelper<AnyRef>
{
    using StorageType = AnyRef;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyRef &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    const AnyRef &Get(const AnyRef &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, const AnyRef &value) const
    {
        Set(hyp_data, AnyRef(value));
    }

    void Set(HypData &hyp_data, AnyRef &&value) const
    {
        hyp_data.value.Set<AnyRef>(std::move(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        const AnyRef &value = hyp_data.Get<AnyRef>();

        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(value.GetTypeID());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        if (!value.HasValue()) {
            // unset
            out_data = fbom::FBOMData();

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(value, object)) {
            return err;
        }

        object.m_deserialized_object.Emplace(std::move(value));

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<T *, std::enable_if_t< !is_const_pointer<T *> && !std::is_same_v<T *, void *> >> {};

template <class T>
struct HypDataHelper<T *, std::enable_if_t< !is_const_pointer<T *> && !std::is_same_v<T *, void *> >> : HypDataHelper<AnyRef>
{
    using ConvertibleFrom = Tuple<AnyHandle, RC<void>>;

    HYP_FORCE_INLINE bool Is(const AnyRef &value) const
    {
        return value.Is<T>();
    }

    // HYP_FORCE_INLINE bool Is(const Any &value) const
    // {
    //     return value.Is<T>();
    // }

    HYP_FORCE_INLINE bool Is(const AnyHandle &value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(const RC<void> &value) const
    {
        return value.Is<T>();
    }

    T *Get(const AnyRef &value) const
    {
        AssertThrow(value.Is<T>());

        return static_cast<T *>(value.GetPointer());
    }

    // T *Get(const Any &value) const
    // {
    //     AssertThrow(value.Is<T>());

    //     return value.Get<T>();
    // }

    T *Get(const AnyHandle &value) const
    {
        AssertThrow(value.Is<T>());

        return value.TryGet<T>();
    }

    T *Get(const RC<void> &value) const
    {
        AssertThrow(value.Is<T>());

        return value.CastUnsafe<T>();
    }

    void Set(HypData &hyp_data, T *value) const
    {
        HypDataHelper<AnyRef>::Set(hyp_data, AnyRef(TypeID::ForType<T>(), value));
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data) {
            out = HypData(static_cast<T *>(nullptr));

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(object)) {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(object, out)) {
            return err;
        }

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<const T *, std::enable_if_t< !std::is_same_v<T *, void *> >> {};

template <class T>
struct HypDataHelper<const T *, std::enable_if_t< !std::is_same_v<T *, void *> >> : HypDataHelper<T *>
{
    const T *Get(const ConstAnyRef &value) const
    {
        return HypDataHelper<T *>::Get(AnyRef(value.GetTypeID(), const_cast<void *>(value.GetPointer())));
    }

    const T *Get(const AnyRef &value) const
    {
        return HypDataHelper<T *>::Get(value);
    }

    const T *Get(const AnyHandle &value) const
    {
        return HypDataHelper<T *>::Get(value);
    }

    const T *Get(const RC<void> &value) const
    {
        return HypDataHelper<T *>::Get(value);
    }

    void Set(HypData &hyp_data, const T *value) const
    {
        HypDataHelper<T *>::Set(hyp_data, const_cast<T *>(value));
    }
};

template <>
struct HypDataHelperDecl<Any> {};

template <>
struct HypDataHelper<Any>
{
    using StorageType = Any;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    Any &Get(Any &value) const
    {
        return value;
    }

    const Any &Get(const Any &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, Any &&value) const
    {
        hyp_data.value.Set<Any>(std::move(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();

        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(value.GetTypeID());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        if (!value.HasValue()) {
            // unset
            out_data = fbom::FBOMData();

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(value.ToRef(), object)) {
            return err;
        }

        object.m_deserialized_object.Emplace(std::move(value));

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }
};

template <int StringType>
struct HypDataHelperDecl<containers::detail::String<StringType>> {};

template <int StringType>
struct HypDataHelper<containers::detail::String<StringType>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<containers::detail::String<StringType>>();
    }

    containers::detail::String<StringType> &Get(Any &value) const
    {
        return value.Get<containers::detail::String<StringType>>();
    }

    const containers::detail::String<StringType> &Get(const Any &value) const
    {
        return value.Get<containers::detail::String<StringType>>();
    }

    void Set(HypData &hyp_data, const containers::detail::String<StringType> &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<containers::detail::String<StringType>>(value));
    }

    void Set(HypData &hyp_data, containers::detail::String<StringType> &&value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<containers::detail::String<StringType>>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();

        if (auto string_opt = hyp_data.TryGet<containers::detail::String<StringType>>()) {
            out_data = fbom::FBOMData::FromString(*string_opt);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize string - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        containers::detail::String<StringType> result;

        if (fbom::FBOMResult err = data.ReadString(result)) {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <int StringType>
struct HypDataHelperDecl<utilities::detail::StringView<StringType>> {};

template <int StringType>
struct HypDataHelper<utilities::detail::StringView<StringType>> : HypDataHelper<containers::detail::String<StringType>>
{
    using ConvertibleFrom = Tuple<containers::detail::String<StringType>>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return HypDataHelper<containers::detail::String<StringType>>::Is(value);
    }

    utilities::detail::StringView<StringType> Get(const Any &value) const
    {
        return HypDataHelper<containers::detail::String<StringType>>::Get(value);
    }

    void Set(HypData &hyp_data, const utilities::detail::StringView<StringType> &value) const
    {
        HypDataHelper<containers::detail::String<StringType>>::Set(hyp_data, value);
    }
};

template <>
struct HypDataHelperDecl<Name> {};

template <>
struct HypDataHelper<Name> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<ANSIString>();
    }

    HYP_FORCE_INLINE Name Get(const Any &value) const
    {
        return CreateNameFromDynamicString(value.Get<ANSIString>());
    }

    HYP_FORCE_INLINE void Set(HypData &hyp_data, Name value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<ANSIString>(value.LookupString()));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();

        if (auto name_opt = hyp_data.TryGet<Name>()) {
            out_data = fbom::FBOMData::FromString(ANSIString(name_opt->LookupString()));

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize name string - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        ANSIString result;

        if (fbom::FBOMResult err = data.ReadString(result)) {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};



template <>
struct HypDataHelperDecl<WeakName> {};

template <>
struct HypDataHelper<WeakName> : HypDataHelper<Name>
{
};

template <class T, SizeType NumInlineBytes>
struct HypDataHelperDecl<Array<T, NumInlineBytes>> {};

template <class T, SizeType NumInlineBytes>
struct HypDataHelper<Array<T, NumInlineBytes>, std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<Array<T, NumInlineBytes>>();
    }

    Array<T, NumInlineBytes> &Get(const Any &value) const
    {
        return value.Get<Array<T, NumInlineBytes>>();
    }

    void Set(HypData &hyp_data, const Array<T, NumInlineBytes> &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Array<T, NumInlineBytes>>(value));
    }

    void Set(HypData &hyp_data, Array<T, NumInlineBytes> &&value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Array<T, NumInlineBytes>>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &any_value = hyp_data.Get<Any>();
        
        Array<T, NumInlineBytes> *array_ptr = any_value.TryGet<Array<T, NumInlineBytes>>();

        if (!array_ptr) {
            return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize array - HypData is not expected array type" };
        }

        const SizeType size = array_ptr->Size();

        if (size == 0) {
            // If size is empty, serialize a placeholder value to get the element type
            out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(detail::HypDataPlaceholderSerializedType<T>::Get()));

            return fbom::FBOMResult::FBOM_OK;
        }

        Array<fbom::FBOMData> elements;
        elements.Resize(size);

        for (SizeType i = 0; i < size; i++) {
            if (fbom::FBOMResult err = HypDataHelper<T>::Serialize(HypData((*array_ptr)[i]), elements[i])) {
                return err;
            }
        }

        out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(elements[0].GetType(), std::move(elements)));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        fbom::FBOMArray array { fbom::FBOMUnset() };

        if (fbom::FBOMResult err = data.ReadArray(array)) {
            return err;
        }

        const SizeType size = array.Size();

        Array<T, NumInlineBytes> result;
        result.Reserve(size);

        for (SizeType i = 0; i < size; i++) {
            HypData element;

            if (fbom::FBOMResult err = HypDataHelper<T>::Deserialize(array.GetElement(i), element)) {
                return err;
            }

            result.PushBack(element.Get<T>());
        }

        HypDataHelper<Array<T, NumInlineBytes>>{}.Set(out, std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class T, SizeType Size>
struct HypDataHelperDecl<FixedArray<T, Size>> {};

template <class T, SizeType Size>
struct HypDataHelper<FixedArray<T, Size>, std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<FixedArray<T, Size>>();
    }

    FixedArray<T, Size> &Get(const Any &value) const
    {
        return value.Get<FixedArray<T, Size>>();
    }
    
    void Set(HypData &hyp_data, const FixedArray<T, Size> &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<FixedArray<T, Size>>(value));
    }

    void Set(HypData &hyp_data, FixedArray<T, Size> &&value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<FixedArray<T, Size>>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();
        
        FixedArray<T, Size> *array_ptr = value.TryGet<FixedArray<T, Size>>();

        if (!array_ptr) {
            return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize array - HypData is not expected array type" };
        }

        if (Size == 0) {
            // If size is empty, serialize a placeholder value to get the element type
            out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(detail::HypDataPlaceholderSerializedType<T>::Get()));

            return fbom::FBOMResult::FBOM_OK;
        }

        Array<fbom::FBOMData> elements;
        elements.Resize(Size);

        for (SizeType i = 0; i < Size; i++) {
            if (fbom::FBOMResult err = HypDataHelper<T>::Serialize(HypData((*array_ptr)[i]), elements[i])) {
                return err;
            }
        }

        out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(elements[0].GetType(), std::move(elements)));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        fbom::FBOMArray array { fbom::FBOMUnset() };

        if (fbom::FBOMResult err = data.ReadArray(array)) {
            return err;
        }

        if (Size != array.Size()) {
            return { fbom::FBOMResult::FBOM_ERR, "Failed to deserialize array - size does not match expected size" };
        }

        FixedArray<T, Size> result;

        for (SizeType i = 0; i < Size; i++) {
            HypData element;

            if (fbom::FBOMResult err = HypDataHelper<T>::Deserialize(array.GetElement(i), element)) {
                return err;
            }

            result[i] = element.Get<T>();
        }

        HypDataHelper<FixedArray<T, Size>>{}.Set(out, std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class T, SizeType Size>
struct HypDataHelperDecl<T[Size]> {};

template <class T, SizeType Size>
struct HypDataHelper<T[Size], std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<FixedArray<T, Size>>
{
    using ConvertibleFrom = Tuple<FixedArray<T, Size>>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<FixedArray<T, Size>>();
    }

    FixedArray<T, Size> &Get(const Any &value) const
    {
        return value.Get<FixedArray<T, Size>>();
    }

    void Set(HypData &hyp_data, const T (&value)[Size]) const
    {
        HypDataHelper<FixedArray<T, Size>>::Set(hyp_data, MakeFixedArray(value));
    }
};

template <class K, class V>
struct HypDataHelperDecl<Pair<K, V>> {};

template <class K, class V>
struct HypDataHelper<Pair<K, V>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<Pair<K, V>>();
    }

    Pair<K, V> &Get(const Any &value) const
    {
        return value.Get<Pair<K, V>>();
    }
    
    void Set(HypData &hyp_data, const Pair<K, V> &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Pair<K, V>>(value));
    }

    void Set(HypData &hyp_data, Pair<K, V> &&value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Pair<K, V>>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();
        
        Pair<K, V> *pair_ptr = value.TryGet<Pair<K, V>>();

        if (!pair_ptr) {
            return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize pair - HypData is not expected type" };
        }

        fbom::FBOMData first_data;
        fbom::FBOMData second_data;

        if (fbom::FBOMResult err = HypDataHelper<K>::Serialize(HypData(pair_ptr->first), first_data)) {
            return err;
        }

        if (fbom::FBOMResult err = HypDataHelper<V>::Serialize(HypData(pair_ptr->second), second_data)) {
            return err;
        }
        
        fbom::FBOMObject object(fbom::FBOMObjectType(TypeWrapper<Pair<K, V>> { }, FBOMTypeFlags::DEFAULT, fbom::FBOMBaseObjectType()));
        object.SetProperty("Key", std::move(first_data));
        object.SetProperty("Value", std::move(second_data));

        out_data = fbom::FBOMData::FromObject(std::move(object));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(object)) {
            return err;
        }

        HypData first;
        HypData second;

        if (fbom::FBOMResult err = HypDataHelper<K>::Deserialize(object.GetProperty("Key"), first)) {
            return err;
        }

        if (fbom::FBOMResult err = HypDataHelper<V>::Deserialize(object.GetProperty("Value"), second)) {
            return err;
        }

        HypDataHelper<Pair<K, V>>{}.Set(out, Pair<K, V> { first.Get<K>(), second.Get<V>() });

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class K, class V>
struct HypDataHelperDecl<HashMap<K, V>> {};

template <class K, class V>
struct HypDataHelper<HashMap<K, V>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<HashMap<K, V>>();
    }

    HashMap<K, V> &Get(const Any &value) const
    {
        return value.Get<HashMap<K, V>>();
    }

    void Set(HypData &hyp_data, const HashMap<K, V> &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<HashMap<K, V>>(value));
    }

    void Set(HypData &hyp_data, HashMap<K, V> &&value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<HashMap<K, V>>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &any_value = hyp_data.Get<Any>();
        
        HashMap<K, V> *hash_map_ptr = any_value.TryGet<HashMap<K, V>>();

        if (!hash_map_ptr) {
            return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize HashMap - HypData is not expected type" };
        }

        const SizeType size = hash_map_ptr->Size();

        if (size == 0) {
            // If size is empty, serialize a placeholder value to get the element type
            out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(detail::HypDataPlaceholderSerializedType<Pair<K, V>>::Get()));

            return fbom::FBOMResult::FBOM_OK;
        }

        Array<fbom::FBOMData> elements;
        elements.Reserve(size);

        uint32 element_index = 0;
        
        for (const Pair<K, V> &pair : *hash_map_ptr) {
            fbom::FBOMData &element = elements.EmplaceBack();

            if (fbom::FBOMResult err = HypDataHelper<Pair<K, V>>::Serialize(HypData(pair), element)) {
                return err;
            }
        }

        out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(elements[0].GetType(), std::move(elements)));

        return fbom::FBOMResult::FBOM_OK;
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        fbom::FBOMArray array { fbom::FBOMUnset() };

        if (fbom::FBOMResult err = data.ReadArray(array)) {
            return err;
        }

        const SizeType size = array.Size();

        HashMap<K, V> result;

        for (SizeType i = 0; i < size; i++) {
            HypData element;

            if (fbom::FBOMResult err = HypDataHelper<Pair<K, V>>::Deserialize(array.GetElement(i), element)) {
                return err;
            }

            result.Insert(element.Get<Pair<K, V>>());
        }

        HypDataHelper<HashMap<K, V>>{}.Set(out, std::move(result));

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
struct HypDataHelperDecl<math::detail::Vec2<T>> {};

template <class T>
struct HypDataHelper<math::detail::Vec2<T>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<math::detail::Vec2<T>>();
    }

    math::detail::Vec2<T> &Get(Any &value) const
    {
        return value.Get<math::detail::Vec2<T>>();
    }

    const math::detail::Vec2<T> &Get(const Any &value) const
    {
        return value.Get<math::detail::Vec2<T>>();
    }

    void Set(HypData &hyp_data, const math::detail::Vec2<T> &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<math::detail::Vec2<T>>(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();

        if (math::detail::Vec2<T> *ptr = value.TryGet<math::detail::Vec2<T>>()) {
            out_data = fbom::FBOMData(*ptr);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize Vector type - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        math::detail::Vec2<T> result;

        if (fbom::FBOMResult err = data.Read(&result)) {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class T>
struct HypDataHelperDecl<math::detail::Vec3<T>> {};

template <class T>
struct HypDataHelper<math::detail::Vec3<T>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<math::detail::Vec3<T>>();
    }

    math::detail::Vec3<T> &Get(Any &value) const
    {
        return value.Get<math::detail::Vec3<T>>();
    }

    const math::detail::Vec3<T> &Get(const Any &value) const
    {
        return value.Get<math::detail::Vec3<T>>();
    }

    void Set(HypData &hyp_data, const math::detail::Vec3<T> &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<math::detail::Vec3<T>>(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();

        if (math::detail::Vec3<T> *ptr = value.TryGet<math::detail::Vec3<T>>()) {
            out_data = fbom::FBOMData(*ptr);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize Vector type - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        math::detail::Vec3<T> result;

        if (fbom::FBOMResult err = data.Read(&result)) {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class T>
struct HypDataHelperDecl<math::detail::Vec4<T>> {};

template <class T>
struct HypDataHelper<math::detail::Vec4<T>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<math::detail::Vec4<T>>();
    }

    math::detail::Vec4<T> &Get(Any &value) const
    {
        return value.Get<math::detail::Vec4<T>>();
    }

    const math::detail::Vec4<T> &Get(const Any &value) const
    {
        return value.Get<math::detail::Vec4<T>>();
    }

    void Set(HypData &hyp_data, const math::detail::Vec4<T> &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<math::detail::Vec4<T>>(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();

        if (math::detail::Vec4<T> *ptr = value.TryGet<math::detail::Vec4<T>>()) {
            out_data = fbom::FBOMData(*ptr);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize Vector type - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        math::detail::Vec4<T> result;

        if (fbom::FBOMResult err = data.Read(&result)) {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Matrix3> {};

template <>
struct HypDataHelper<Matrix3> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<Matrix3>();
    }

    Matrix3 &Get(Any &value) const
    {
        return value.Get<Matrix3>();
    }

    const Matrix3 &Get(const Any &value) const
    {
        return value.Get<Matrix3>();
    }

    void Set(HypData &hyp_data, const Matrix3 &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Matrix3>(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();

        if (Matrix3 *ptr = value.TryGet<Matrix3>()) {
            out_data = fbom::FBOMData(*ptr);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize Matrix3 - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        Matrix3 result;

        if (fbom::FBOMResult err = data.Read(&result)) {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Matrix4> {};

template <>
struct HypDataHelper<Matrix4> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<Matrix4>();
    }

    Matrix4 &Get(Any &value) const
    {
        return value.Get<Matrix4>();
    }

    const Matrix4 &Get(const Any &value) const
    {
        return value.Get<Matrix4>();
    }

    void Set(HypData &hyp_data, const Matrix4 &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Matrix4>(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();

        if (Matrix4 *ptr = value.TryGet<Matrix4>()) {
            out_data = fbom::FBOMData(*ptr);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize Matrix4 - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        Matrix4 result;

        if (fbom::FBOMResult err = data.Read(&result)) {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Quaternion> {};

template <>
struct HypDataHelper<Quaternion> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<Quaternion>();
    }

    Quaternion &Get(Any &value) const
    {
        return value.Get<Quaternion>();
    }

    const Quaternion &Get(const Any &value) const
    {
        return value.Get<Quaternion>();
    }

    void Set(HypData &hyp_data, const Quaternion &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<Quaternion>(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();

        if (Quaternion *ptr = value.TryGet<Quaternion>()) {
            out_data = fbom::FBOMData(*ptr);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize Quaternion - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        Quaternion result;

        if (fbom::FBOMResult err = data.Read(&result)) {
            return err;
        }

        out = HypData(std::move(result));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<ByteBuffer> {};

template <>
struct HypDataHelper<ByteBuffer> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<ByteBuffer>();
    }

    ByteBuffer &Get(const Any &value) const
    {
        return value.Get<ByteBuffer>();
    }

    void Set(HypData &hyp_data, const ByteBuffer &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<ByteBuffer>(value));
    }

    void Set(HypData &hyp_data, ByteBuffer &&value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<ByteBuffer>(std::move(value)));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        HYP_SCOPE;
        
        Any &value = hyp_data.Get<Any>();

        if (ByteBuffer *ptr = value.TryGet<ByteBuffer>()) {
            out_data = fbom::FBOMData::FromByteBuffer(*ptr);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize ByteBuffer - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        ByteBuffer byte_buffer;

        if (fbom::FBOMResult err = data.ReadByteBuffer(byte_buffer)) {
            return err;
        }

        out = HypData(std::move(byte_buffer));

        return { fbom::FBOMResult::FBOM_OK };
    }
};

template <class T>
struct HypDataHelper<T, std::enable_if_t<!HypData::can_store_directly<T> && !implementation_exists<HypDataHelperDecl<T>>>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<T *, AnyRef, AnyHandle, RC<void>>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(T *value) const
    {
        // Dereferencing a null pointer would be bad - so we'll just pretend it's not the type
        return value != nullptr;
    }

    HYP_FORCE_INLINE bool Is(const AnyRef &value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(const AnyHandle &value) const
    {
        if constexpr (has_opaque_handle_defined<T>) {
            return value.Is<T>();
        } else {
            return false;
        }
    }

    HYP_FORCE_INLINE bool Is(const RC<void> &value) const
    {
        return value.Is<T>();
    }

    T &Get(const Any &value) const
    {
        return value.Get<T>();
    }

    T &Get(T *value) const
    {
        return *value;
    }

    T &Get(const AnyRef &value) const
    {
        return value.Get<T>();
    }

    T &Get(const AnyHandle &value) const
    {
        if constexpr (has_opaque_handle_defined<T>) {
            return *value.Cast<T>().Get();
        } else {
            HYP_UNREACHABLE();
        }
    }

    T &Get(const RC<void> &value) const
    {
        return *value.Cast<T>();
    }

    void Set(HypData &hyp_data, const T &value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<T>(value));
    }

    void Set(HypData &hyp_data, T &&value) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<T>(std::move(value)));
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        HYP_SCOPE;
        
        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data) {
            out = HypData(T { });

            return fbom::FBOMResult::FBOM_OK;
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = data.ReadObject(object)) {
            return err;
        }

        if (fbom::FBOMResult err = marshal->Deserialize(object, out)) {
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
    HYP_FORCE_INLINE bool operator()(const HypData::VariantType &value) const
    {
        constexpr bool should_skip_additional_is_check = std::is_same_v<T, typename HypDataHelper<T>::StorageType>;

        static_assert(HypData::can_store_directly<typename HypDataHelper<T>::StorageType>, "StorageType must be a type that can be stored directly in the HypData variant without allocating memory dynamically");

        return value.Is<typename HypDataHelper<T>::StorageType>()
            && (should_skip_additional_is_check || HypDataHelper<T>{}.Is(value.Get<typename HypDataHelper<T>::StorageType>()));
    }
};

template <class T, class... ConvertibleFrom>
struct HypDataTypeChecker_Tuple<T, Tuple<ConvertibleFrom...>>
{
    HYP_FORCE_INLINE bool operator()(const HypData::VariantType &value) const
    {
        return HypDataTypeChecker<T>{}(value) || (HypDataTypeChecker<ConvertibleFrom>{}(value) || ...);
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

        if (!is_init) {
            is_init = true;
            
            fbom::FBOMData placeholder_data;

            if (fbom::FBOMResult err = HypDataHelper<T>::Serialize(HypData(T { }), placeholder_data)) {
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
HYP_FORCE_INLINE bool HypDataGetter_Tuple_Impl(VariantType &&value, Optional<ReturnType> &out_value, std::index_sequence<Indices...>)
{
    const auto GetForTypeIndex = [&value]<SizeType SelectedTypeIndex>(Optional<ReturnType> &out_value, std::integral_constant<SizeType, SelectedTypeIndex>) -> bool
    {
        using SelectedType = typename TupleElement<SelectedTypeIndex, Types...>::Type;
        using StorageType = typename HypDataHelper<SelectedType>::StorageType;

        static_assert(HypData::can_store_directly<typename HypDataHelper<NormalizedType<ReturnType>>::StorageType>);

        if constexpr (std::is_same_v<NormalizedType<ReturnType>, StorageType>) {
            out_value.Set(value.template Get<StorageType>());
        } else {
            decltype(auto) internal_value = value.template Get<StorageType>();

            if (!HypDataHelper<NormalizedType<ReturnType>>{}.Is(internal_value)) {
                return false;
            }
            
            out_value.Set(HypDataHelper<NormalizedType<ReturnType>>{}.Get(std::forward<decltype(internal_value)>(internal_value)));
        }

        return true;
    };

    return ((HypDataTypeChecker<Types>{}(value) && GetForTypeIndex(out_value, std::integral_constant<SizeType, Indices>{})) || ...);
}

template <class ReturnType, class T, class... ConvertibleFrom>
struct HypDataGetter_Tuple<ReturnType, T, Tuple<ConvertibleFrom...>>
{
    HYP_FORCE_INLINE bool operator()(HypData::VariantType &value, Optional<ReturnType> &out_value) const
    {
        return HypDataGetter_Tuple_Impl<HypData::VariantType &, ReturnType, T, ConvertibleFrom...>(value, out_value, std::index_sequence_for<T, ConvertibleFrom...>{});
    }

    HYP_FORCE_INLINE bool operator()(const HypData::VariantType &value, Optional<ReturnType> &out_value) const
    {
        return HypDataGetter_Tuple_Impl<const HypData::VariantType &, ReturnType, T, ConvertibleFrom...>(value, out_value, std::index_sequence_for<T, ConvertibleFrom...>{});
    }
};

#pragma endregion HypDataGetter implementation

} // namespace detail

static_assert(sizeof(HypData) == 32, "sizeof(HypData) must match C# struct size");

} // namespace hyperion

#endif