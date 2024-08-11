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
        Any
    > value;

    template <class T>
    static constexpr bool can_store_directly = std::is_same_v<T, int8>
        || std::is_same_v<T, int16>
        || std::is_same_v<T, int32>
        || std::is_same_v<T, int64>
        || std::is_same_v<T, uint8>
        || std::is_same_v<T, uint16>
        || std::is_same_v<T, uint32>
        || std::is_same_v<T, uint64>
        || std::is_same_v<T, float>
        || std::is_same_v<T, double>
        || std::is_same_v<T, bool>
        || std::is_base_of_v<IDBase, T>
        || std::is_base_of_v<HandleBase, T>
        || std::is_same_v<T, AnyHandle>
        || std::is_base_of_v<typename RC<void>::RefCountedPtrBase, T>;

    HypData()                                       = default;

    template <class T>
    HypData(T &&value)
    {
        detail::HypDataInitializer<NormalizedType<T>>{}.Set(*this, std::forward<T>(value));

        // if constexpr (CanStoreDirectly<NormalizedType<T>>()) {
        //     this->value.Set(std::forward<T>(value));
        // } else {
        //     this->value.Set(Any(std::forward<T>(value)));
        // }
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
        
        if constexpr (can_store_directly<typename detail::HypDataInitializer<T>::StorageType>) {
            return value.Is<typename detail::HypDataInitializer<T>::StorageType>();
        } else {
            if (const Any *any_ptr = value.TryGet<Any>()) {
                return any_ptr->Is<typename detail::HypDataInitializer<T>::StorageType>()
                    && detail::HypDataInitializer<NormalizedType<T>>{}.Is(any_ptr->Get<typename detail::HypDataInitializer<T>::StorageType>());
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

    // template <class T>
    // HYP_FORCE_INLINE auto *TryGet()
    // {
    //     if (IsValid()) {
    //         if constexpr (can_store_directly<typename detail::HypDataInitializer<T>::StorageType>) {
    //             if (auto *ptr = value.TryGet<typename detail::HypDataInitializer<T>::StorageType>()) {
    //                 return &detail::HypDataInitializer<NormalizedType<T>>{}.Get(*ptr);
    //             }
    //         } else {
    //             if (Any *any_ptr = value.TryGet<Any>()) {
    //                 if (auto *ptr = any_ptr->TryGet<typename detail::HypDataInitializer<T>::StorageType>()) {
    //                     return &detail::HypDataInitializer<NormalizedType<T>>{}.Get(*ptr);
    //                 }
    //             }
    //         }
    //     }

    //     return nullptr;
    // }

    // template <class T>
    // HYP_FORCE_INLINE const auto *TryGet() const
    // {
    //     if (IsValid()) {
    //         if constexpr (can_store_directly<typename detail::HypDataInitializer<T>::StorageType>) {
    //             if (const auto *ptr = value.TryGet<typename detail::HypDataInitializer<T>::StorageType>()) {
    //                 return &detail::HypDataInitializer<NormalizedType<T>>{}.Get(*ptr);
    //             }
    //         } else {
    //             if (const Any *any_ptr = value.TryGet<Any>()) {
    //                 if (const auto *ptr = any_ptr->TryGet<typename detail::HypDataInitializer<T>::StorageType>()) {
    //                     return &detail::HypDataInitializer<NormalizedType<T>>{}.Get(*ptr);
    //                 }
    //             }
    //         }
    //     }

    //     return nullptr;
    // }
};

namespace detail {

template <class T>
struct HypDataInitializer<T, std::enable_if_t<std::is_fundamental_v<T>>>
{
    using StorageType = T;

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
struct HypDataInitializer<T, std::enable_if_t<!HypData::can_store_directly<T>>>
{
    using StorageType = T;

    constexpr bool Is(const T &value) const
    {
        return true;
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

#if 0

namespace detail {

template <class T, class T2>
struct ConditionalType_Impl;

template <class T>
struct ConditionalType_Impl<T, std::false_type>;

template <class T>
struct ConditionalType_Impl<T, std::true_type>
{
    using Type = T;
};

} // namespace detail

template <class T, bool Condition>
using ConditionalType = typename detail::ConditionalType_Impl<T, std::bool_constant< Condition > >::Type;


enum class HypDataArchetype : uint32
{
    INVALID     = uint32(-1),
    EMPTY       = 0,
    VALUE_TYPE,
    REFERENCE_TYPE,
    ENUM_TYPE,
    STRUCT_TYPE,

    HYP_OBJECT_ID,
    HYP_OBJECT_HANDLE
};

class HypData;

namespace detail {

template <class T, class T2 = void>
struct HypDataArchetype_Impl;

template <HypDataArchetype Archetype, class T>
struct HypData_Impl;


} // namespace detail

using HypDataGetValueFunction = void(*)(void *, void **);
using HypDataDestructFunction = void(*)(void *);

struct HypData
{
    static constexpr SizeType buffer_size = 128;

    HypDataArchetype                archetype = HypDataArchetype::INVALID;
    TypeID                          type_id = TypeID::Void();
    HypDataDestructFunction         destruct_fn = nullptr;
    alignas(std::max_align_t) ubyte buffer[buffer_size];

    HypData() = default;

    template <class T>
    HypData(T &&value)
    {
        if constexpr (implementation_exists<detail::HypDataArchetype_Impl<T>>) {
            using ArchetypeImpl = detail::HypDataArchetype_Impl<T>;
            using Impl = detail::HypData_Impl<ArchetypeImpl::value, T>;

            Impl::Initialize(&buffer[0], std::forward<T>(value));

            archetype = ArchetypeImpl::value;
            type_id = TypeID::ForType<NormalizedType<T>>();
            destruct_fn = &Impl::Destruct;
        } else {
            static_assert(implementation_exists<detail::HypDataArchetype_Impl<T>>, "Type not supported for HypData");
        }
    }

    HypData(const HypData &other)               = delete;
    HypData &operator=(const HypData &other)    = delete;

    HypData(HypData &&other) noexcept
        : archetype(other.archetype),
          type_id(other.type_id),
          destruct_fn(other.destruct_fn)
    {
        Memory::MemCpy(buffer, other.buffer, buffer_size);

        other.archetype = HypDataArchetype::INVALID;
        other.type_id = TypeID::Void();
        other.destruct_fn = nullptr;
    }

    HypData &operator=(HypData &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (destruct_fn != nullptr) {
            destruct_fn(&buffer[0]);
        }

        Memory::MemCpy(buffer, other.buffer, buffer_size);
        archetype = other.archetype;
        type_id = other.type_id;
        destruct_fn = other.destruct_fn;

        other.archetype = HypDataArchetype::INVALID;
        other.type_id = TypeID::Void();
        other.destruct_fn = nullptr;

        return *this;
    }

    ~HypData()
    {
        if (destruct_fn != nullptr) {
            destruct_fn(&buffer[0]);
        }
    }

    template <class T>
    // typename detail::HypData_Impl<detail::HypDataArchetype_Impl<T>::value, T>::Type &Get()
    T &Get()
    {
        AssertThrowMsg(TypeID::ForType<NormalizedType<T>>() == type_id, "TypeID does not match requested type");

        HYP_NOT_IMPLEMENTED();
    }

    template <class T>
    // const typename detail::HypData_Impl<detail::HypDataArchetype_Impl<T>::value, T>::Type &Get() const
    const T &Get() const
    {
        AssertThrowMsg(TypeID::ForType<NormalizedType<T>>() == type_id, "TypeID does not match requested type");

        HYP_NOT_IMPLEMENTED();
    }
};

static_assert(sizeof(HypData) == 144, "sizeof(HypData) must match C# struct size");

namespace detail {

#pragma region VALUE_TYPE

template <class T>
struct HypDataArchetype_Impl<T, std::enable_if_t<std::is_fundamental_v<T>>>
{
    static constexpr HypDataArchetype value = HypDataArchetype::VALUE_TYPE;
};

template <class T>
struct HypData_Impl<HypDataArchetype::VALUE_TYPE, T>
{
    using Type = T;

    static void Initialize(void *buffer_address, T value)
    {
        // static_assert(sizeof(T) <= BufferSize);

        Memory::MemCpy(buffer_address, &value, sizeof(T));
    }

    static void Destruct(void *buffer_address)
    {
        // do nothing
    }
};

#pragma endregion VALUE_TYPE

#pragma region REFERENCE_TYPE

template <class T>
struct HypDataArchetype_Impl<T, std::enable_if_t<std::is_reference_v<T>>>
{
    static constexpr HypDataArchetype value = HypDataArchetype::REFERENCE_TYPE;
};

template <class T>
struct HypData_Impl<HypDataArchetype::REFERENCE_TYPE, T>
{
    using TNonRef = typename std::remove_reference_t<T>;

    using Type = TNonRef;

    static void Initialize(void *buffer_address, TNonRef &value)
    {
        // static_assert(sizeof(T) <= BufferSize);

        const TNonRef *tptr = &value;

        Memory::MemCpy(buffer_address, &tptr, sizeof(TNonRef *));
    }

    static void Destruct(void *buffer_address)
    {
        // do nothing
    }
};

#pragma endregion REFERENCE_TYPE

#pragma region ENUM_TYPE

template <class T>
struct HypDataArchetype_Impl<T, std::enable_if_t<std::is_enum_v<T>>>
{
    static constexpr HypDataArchetype value = HypDataArchetype::ENUM_TYPE;
};

template <class T>
struct HypData_Impl<HypDataArchetype::ENUM_TYPE, T>
{
    using UnderlyingType = std::underlying_type_t<T>;
    
    using Type = T;

    static void Initialize(void *buffer_address, T value)
    {
        // static_assert(sizeof(T) <= BufferSize);

        Memory::MemCpy(buffer_address, &value, sizeof(UnderlyingType));
    }

    static void Destruct(void *buffer_address)
    {
        // do nothing
    }
};

#pragma endregion ENUM_TYPE

#pragma region STRUCT_TYPE

template <class T>
struct HypDataArchetype_Impl<T, std::enable_if_t<std::is_class_v<T> && IsPODType<T>>>
{
    static constexpr HypDataArchetype value = HypDataArchetype::STRUCT_TYPE;
};

template <class T>
struct HypData_Impl<HypDataArchetype::STRUCT_TYPE, T>
{
    using Type = T;

    static void Initialize(void *buffer_address, const T &value)
    {
        // static_assert(sizeof(T) <= BufferSize);

        static_assert(alignof(T) <= alignof(std::max_align_t));

        Memory::MemCpy(buffer_address, &value, sizeof(T));
    }

    static void Destruct(void *buffer_address)
    {
        // do nothing; POD is trivially destructible
    }
};

#pragma endregion STRUCT_TYPE

#pragma region HYP_OBJECT_ID

template <class T>
struct HypDataArchetype_Impl<ID<T>, void>
{
    static constexpr HypDataArchetype value = HypDataArchetype::HYP_OBJECT_ID;
};

template <class T>
struct HypData_Impl<HypDataArchetype::HYP_OBJECT_ID, ID<T>>
{
    using Type = ID<T>;

    static void Initialize(void *buffer_address, const ID<T> &value)
    {
        // static_assert(sizeof(Handle<T>) <= BufferSize);

        Memory::Construct<ID<T>>(buffer_address, value);
    }

    static void Destruct(void *buffer_address)
    {
        // do nothing
    }
};

#pragma endregion HYP_OBJECT_ID

#pragma region HYP_OBJECT_HANDLE

template <class T>
struct HypDataArchetype_Impl<Handle<T>, void>
{
    static constexpr HypDataArchetype value = HypDataArchetype::HYP_OBJECT_HANDLE;
};

template <class T>
struct HypData_Impl<HypDataArchetype::HYP_OBJECT_HANDLE, Handle<T>>
{
    using Type = Handle<T>;

    static void Initialize(void *buffer_address, const Handle<T> &value)
    {
        // static_assert(sizeof(Handle<T>) <= BufferSize);

        Memory::Construct<Handle<T>>(buffer_address, value);
    }

    static void Destruct(void *buffer_address)
    {
        reinterpret_cast<Handle<T> *>(buffer_address)->~Handle<T>();
    }
};

#pragma endregion HYP_OBJECT_HANDLE

} // namespace detail

#endif

} // namespace hyperion

#endif