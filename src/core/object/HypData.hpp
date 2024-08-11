/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_DATA_HPP
#define HYPERION_CORE_HYP_DATA_HPP

#include <core/Defines.hpp>

#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Variant.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

struct HypData;

namespace detail {

template <class T, class T2 = void>
struct HypDataInitializer;

} // namespace detail

struct HypData
{
    Variant<
        int8,
        int16,
        int32,
        int64,
        uint8,
        uint16,
        uint32,
        uint64,
        float,
        double,
        bool,
        IDBase,
        AnyHandle,
        RC<void>,
        AnyRef,
        Any
    > value;

    template <class T>
    static constexpr bool can_store_directly =
        /* Fundamental types - can be stored inline */
        std::is_same_v<T, int8> || std::is_same_v<T, int16> | std::is_same_v<T, int32> | std::is_same_v<T, int64>
        || std::is_same_v<T, uint8> || std::is_same_v<T, uint16> || std::is_same_v<T, uint32> || std::is_same_v<T, uint64>
        || std::is_same_v<T, float> || std::is_same_v<T, double>
        || std::is_same_v<T, bool>
        
        /*! All ID<T> are stored as IDBase */
        || std::is_base_of_v<IDBase, T>

        /*! Handle<T> gets stored as AnyHandle, which holds TypeID for conversion */
        || std::is_base_of_v<HandleBase, T> || std::is_same_v<T, AnyHandle>

        /*! RC<T> gets stored as RC<void> and can be converted back */
        || std::is_base_of_v<typename RC<void>::RefCountedPtrBase, T>

        /*! Pointers are stored as AnyRef which holds TypeID for conversion */
        || std::is_same_v<T, AnyRef> || std::is_pointer_v<T>;

    HypData()                                       = default;

    template <class T>
    HypData(T &&value)
    {
        detail::HypDataInitializer<NormalizedType<T>>{}.Set(*this, std::forward<T>(value));
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

    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        if (!value.IsValid()) {
            return false;
        }

        // if the struct's StorageType typedef is the same as T, we don't need to perform another check.
        constexpr bool should_skip_additional_is_check = std::is_same_v<T, typename detail::HypDataInitializer<T>::StorageType>;
        
        if constexpr (can_store_directly<typename detail::HypDataInitializer<T>::StorageType>) {
            return value.Is<typename detail::HypDataInitializer<T>::StorageType>()
                && (should_skip_additional_is_check || detail::HypDataInitializer<T>{}.Is(value.Get<typename detail::HypDataInitializer<T>::StorageType>()));
        } else {
            if (const Any *any_ptr = value.TryGet<Any>()) {
                return any_ptr->Is<typename detail::HypDataInitializer<T>::StorageType>()
                    && (should_skip_additional_is_check || detail::HypDataInitializer<T>{}.Is(any_ptr->Get<typename detail::HypDataInitializer<T>::StorageType>()));
            }

            return false;
        }
    }

    template <class T>
    HYP_FORCE_INLINE decltype(auto) Get()
    {
        if constexpr (can_store_directly<typename detail::HypDataInitializer<T>::StorageType>) {
            return detail::HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<typename detail::HypDataInitializer<T>::StorageType>());
        } else {
            return detail::HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<Any>().Get<typename detail::HypDataInitializer<T>::StorageType>());
        }
    }

    template <class T>
    HYP_FORCE_INLINE decltype(auto) Get() const
    {
        if constexpr (can_store_directly<typename detail::HypDataInitializer<T>::StorageType>) {
            return detail::HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<typename detail::HypDataInitializer<T>::StorageType>());
        } else {
            return detail::HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<Any>().Get<typename detail::HypDataInitializer<T>::StorageType>());
        }
    }
};

namespace detail {

template <class T>
struct HypDataInitializer<T, std::enable_if_t<std::is_fundamental_v<T>>>
{
    using StorageType = T;

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
};

template <class T>
struct HypDataInitializer<ID<T>>
{
    using StorageType = IDBase;

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
        hyp_data.value.Set<IDBase>(static_cast<const IDBase &>(value));
    }
};

template <>
struct HypDataInitializer<IDBase>
{
    using StorageType = IDBase;

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
};

template <class T>
struct HypDataInitializer<Handle<T>>
{
    using StorageType = AnyHandle;

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
        hyp_data.value.Set<AnyHandle>(value);
    }

    void Set(HypData &hyp_data, Handle<T> &&value) const
    {
        hyp_data.value.Set<AnyHandle>(std::move(value));
    }
};

template <>
struct HypDataInitializer<AnyHandle>
{
    using StorageType = AnyHandle;

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
};

template <class T>
struct HypDataInitializer<RC<T>, std::enable_if_t< !std::is_void_v<T> >>
{
    using StorageType = RC<void>;

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
        hyp_data.value.Set<RC<void>>(value.template Cast<void>());
    }

    void Set(HypData &hyp_data, RC<T> &&value) const
    {
        hyp_data.value.Set<RC<void>>(value.template Cast<void>());
    }
};

template <>
struct HypDataInitializer<RC<void>>
{
    using StorageType = RC<void>;

    HYP_FORCE_INLINE bool Is(const RC<void> &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
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
};

template <class T>
struct HypDataInitializer<T *, std::enable_if_t< !is_const_pointer<T *> && !std::is_same_v<T *, void *> >>
{
    using StorageType = AnyRef;

    HYP_FORCE_INLINE bool Is(const AnyRef &value) const
    {
        return value.Is<T>();
    }

    T *Get(const AnyRef &value) const
    {
        AssertThrow(value.Is<T>());

        return static_cast<T *>(value.GetPointer());
    }

    void Set(HypData &hyp_data, T *value) const
    {
        hyp_data.value.Set<AnyRef>(AnyRef(TypeID::ForType<T>(), value));
    }
};

template <>
struct HypDataInitializer<AnyRef>
{
    using StorageType = AnyRef;

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
        hyp_data.value.Set<AnyRef>(value);
    }

    void Set(HypData &hyp_data, AnyRef &&value) const
    {
        hyp_data.value.Set<AnyRef>(std::move(value));
    }
};

template <class T>
struct HypDataInitializer<T, std::enable_if_t<!HypData::can_store_directly<T>>>
{
    using StorageType = T;

    HYP_FORCE_INLINE bool Is(const T &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    T &Get(T &value) const
    {
        return value;
    }

    const T &Get(const T &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, const T &value) const
    {
        hyp_data.value.Set(Any(value));
    }

    void Set(HypData &hyp_data, T &&value) const
    {
        hyp_data.value.Set(Any(std::move(value)));
    }
};

} // namespace detail

static_assert(sizeof(HypData) == 32, "sizeof(HypData) must match C# struct size");

} // namespace hyperion

#endif