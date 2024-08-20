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

#include <core/memory/Any.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/ByteBuffer.hpp>

#include <asset/serialization/fbom/FBOMResult.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/FBOMArray.hpp>
#include <asset/serialization/fbom/FBOM.hpp>

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
    using Type = decltype(std::declval<HypDataHelper<T>>().Get(*std::declval<std::conditional_t<IsConst, const T, T> *>()));
};

} // namespace detail

using HypDataSerializeFunction = std::add_pointer_t<fbom::FBOMResult(HypData &&hyp_data, fbom::FBOMData &out_data)>;

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
    HypData(T &&value)
        : HypData()
    {
        HypDataHelper<NormalizedType<T>>{}.Set(*this, std::forward<T>(value));
    }

    HypData(const HypData &other)                   = delete;
    HypData &operator=(const HypData &other)        = delete;

    HypData(HypData &&other) noexcept               = default;
    HypData &operator=(HypData &&other) noexcept    = default;

    ~HypData()                                      = default;

    HYP_FORCE_INLINE TypeID GetTypeID() const
    {
        if (const Any *any_ptr = value.TryGet<Any>()) {
            return any_ptr->GetTypeID();
        }

        return value.GetTypeID();
    }

    HYP_FORCE_INLINE bool IsValid() const
        { return value.IsValid(); }

    HYP_FORCE_INLINE AnyRef ToRef()
    {
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
    {
        if (!value.IsValid()) {
            return ConstAnyRef();
        }

        if (const AnyRef *any_ref_ptr = value.TryGet<AnyRef>()) {
            return ConstAnyRef(*any_ref_ptr);
        }

        if (const Any *any_ptr = value.TryGet<Any>()) {
            return any_ptr->ToRef();
        }

        return ConstAnyRef(value.GetTypeID(), value.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        static_assert(!std::is_same_v<T, HypData>);

        if (!value.IsValid()) {
            return false;
        }
        
        return detail::HypDataTypeChecker_Tuple<T, typename HypDataHelper<T>::ConvertibleFrom>{}(value);
    }

    template <class T>
    HYP_FORCE_INLINE auto Get() -> typename detail::HypDataGetReturnTypeHelper<T, false>::Type
    {
        static_assert(!std::is_same_v<T, HypData>);

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

        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<Normalized>());

        if (!marshal) {
            return NoMarshalRegistered(TypeName<Normalized>());
        }

        fbom::FBOMObject object;

        if (fbom::FBOMResult err = marshal->Serialize(ConstAnyRef(value), object)) {
            return err;
        }

        object.m_deserialized_object.Reset(new HypData(value));

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
            // out = HypData();

            // return fbom::FBOMResult::FBOM_OK;
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

template <class T>
struct HypDataHelperDecl<T, std::enable_if_t<std::is_fundamental_v<T>>> {};

template <class T>
struct HypDataHelper<T, std::enable_if_t<std::is_fundamental_v<T>>>
{
    using StorageType = T;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const T &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    constexpr T Get(T value) const
    {
        return value;
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
struct HypDataHelperDecl<ID<T>, std::enable_if_t<!std::is_same_v<T, Entity>>> {};

template <class T>
struct HypDataHelper<ID<T>, std::enable_if_t<!std::is_same_v<T, Entity>>> : HypDataHelper<IDBase>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const IDBase &value) const
    {
        return true; // can't do anything more to check as IDBase doesn't hold type info.
    }

    ID<T> &Get(IDBase &value) const
    {
        return static_cast<ID<T> &>(value);
    }

    const ID<T> &Get(const IDBase &value) const
    {
        return static_cast<const ID<T> &>(value);
    }

    void Set(HypData &hyp_data, const ID<T> &value) const
    {
        HypDataHelper<IDBase>::Set(hyp_data, static_cast<const IDBase &>(value));
    }
};

template <>
struct HypDataHelperDecl<ID<Entity>> {};

template <>
struct HypDataHelper<ID<Entity>> : HypDataHelper<IDBase>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const IDBase &value) const
    {
        return true; // can't do anything more to check as IDBase doesn't hold type info.
    }

    ID<Entity> &Get(IDBase &value) const
    {
        return static_cast<ID<Entity> &>(value);
    }

    const ID<Entity> &Get(const IDBase &value) const
    {
        return static_cast<const ID<Entity> &>(value);
    }

    void Set(HypData &hyp_data, const ID<Entity> &value) const
    {
        HypDataHelper<IDBase>::Set(hyp_data, static_cast<const IDBase &>(value));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        ID<Entity> entity = static_cast<ID<Entity>>(hyp_data.Get<IDBase>());
        
        return HypDataMarshalHelper::Serialize<ID<Entity>>(entity, out_data);
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        return HypDataMarshalHelper::Deserialize<ID<Entity>>(data, out);
    }
};

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

        object.m_deserialized_object.Reset(new HypData(std::move(value)));

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

        object.m_deserialized_object.Reset(new HypData(std::move(value)));

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
        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
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

        object.m_deserialized_object.Reset(new HypData(std::move(value)));

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
        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
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

        object.m_deserialized_object.Reset(new HypData(std::move(value)));

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

template <class T, SizeType Size>
struct HypDataHelperDecl<T[Size]> {};

template <class T, SizeType Size>
struct HypDataHelper<T[Size], std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<FixedArray<T, Size>>();
    }

    FixedArray<T, Size> &Get(Any &value) const
    {
        return value.Get<FixedArray<T, Size>>();
    }

    const FixedArray<T, Size> &Get(const Any &value) const
    {
        return value.Get<FixedArray<T, Size>>();
    }

    void Set(HypData &hyp_data, const T (&value)[Size]) const
    {
        HypDataHelper<Any>::Set(hyp_data, Any::Construct<FixedArray<T, Size>>(MakeFixedArray(value)));
    }

    static fbom::FBOMResult Serialize(HypData &&hyp_data, fbom::FBOMData &out_data)
    {
        Any &value = hyp_data.Get<Any>();
        
        FixedArray<T, Size> *array_ptr = value.TryGet<FixedArray<T, Size>>();

        if (!array_ptr) {
            return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize array - HypData is not expected array type" };
        }

        if (Size == 0) {
            out_data = fbom::FBOMData::FromArray(fbom::FBOMArray(fbom::FBOMUnset()));

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
        fbom::FBOMArray array { fbom::FBOMUnset() };

        if (fbom::FBOMResult err = data.ReadArray(array)) {
            return err;
        }

        if (Size != array.Size()) {
            return { fbom::FBOMResult::FBOM_ERR, "Failed to deserialize array - size does not match expected size" };
        }

        T result[Size];

        for (SizeType i = 0; i < Size; i++) {
            HypData element;

            if (fbom::FBOMResult err = HypDataHelper<T>::Deserialize(array.GetElement(i), element)) {
                return err;
            }

            result[i] = element.Get<T>();
        }

        HypDataHelper<T[Size]>{}.Set(out, result);

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
        Any &value = hyp_data.Get<Any>();

        if (math::detail::Vec2<T> *vec_ptr = value.TryGet<math::detail::Vec2<T>>()) {
            out_data = fbom::FBOMData(*vec_ptr);

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
        Any &value = hyp_data.Get<Any>();

        if (math::detail::Vec3<T> *vec_ptr = value.TryGet<math::detail::Vec3<T>>()) {
            out_data = fbom::FBOMData(*vec_ptr);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize Vector type - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
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
        Any &value = hyp_data.Get<Any>();

        if (math::detail::Vec4<T> *vec_ptr = value.TryGet<math::detail::Vec4<T>>()) {
            out_data = fbom::FBOMData(*vec_ptr);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize Vector type - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
        math::detail::Vec4<T> result;

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

    ByteBuffer &Get(Any &value) const
    {
        return value.Get<ByteBuffer>();
    }

    const ByteBuffer &Get(const Any &value) const
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
        Any &value = hyp_data.Get<Any>();

        if (ByteBuffer *ptr = value.TryGet<ByteBuffer>()) {
            out_data = fbom::FBOMData::FromByteBuffer(*ptr);

            return fbom::FBOMResult::FBOM_OK;
        }

        return { fbom::FBOMResult::FBOM_ERR, "Failed to serialize ByteBuffer - HypData is not expected type" };
    }

    static fbom::FBOMResult Deserialize(const fbom::FBOMData &data, HypData &out)
    {
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
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any &value) const
    {
        return value.Is<T>();
    }

    T &Get(const Any &value) const
    {
        return value.Get<T>();
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
        const fbom::FBOMMarshalerBase *marshal = fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>());

        if (!marshal) {
            return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
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

        static_assert(HypData::can_store_directly<typename HypDataHelper<T>::StorageType>);

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
    const auto InvokeGetter = [&value]<SizeType SelectedTypeIndex>(Optional<ReturnType> &out_value, std::integral_constant<SizeType, SelectedTypeIndex>) -> bool
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

    return ((HypDataTypeChecker<Types>{}(value) && InvokeGetter(out_value, std::integral_constant<SizeType, Indices>{})) || ...);
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