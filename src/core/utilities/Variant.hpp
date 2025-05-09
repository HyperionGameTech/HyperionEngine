/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_VARIANT_HPP
#define HYPERION_VARIANT_HPP

#include <core/memory/Memory.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/utilities/TypeID.hpp>

#include <core/debug/Debug.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Traits.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

namespace hyperion {

namespace utilities {
namespace detail {

template <class VariantType>
struct TypeIndexHelper;

template <class ... Ts>
struct VariantHelper;

template <class T, class... Ts>
struct VariantHelper<T, Ts...>
{
    template <class SrcType>
    static constexpr bool holds_type = std::is_same_v<T, NormalizedType<SrcType>> || (std::is_same_v<Ts, NormalizedType<SrcType>> || ...);

    template <class SrcType>
    static constexpr bool copy_assignable_to = (std::is_assignable_v<T, const NormalizedType<SrcType> &> || (std::is_assignable_v<Ts, const NormalizedType<SrcType> &> || ...));

    template <class SrcType>
    static constexpr bool move_assignable_to = (std::is_assignable_v<T, NormalizedType<SrcType> &&> || (std::is_assignable_v<Ts, NormalizedType<SrcType> &&> || ...));

    template <class SrcType>
    static constexpr bool copy_constructible_to = (std::is_constructible_v<T, const NormalizedType<SrcType> &> || (std::is_constructible_v<Ts, const NormalizedType<SrcType> &> || ...));

    template <class SrcType>
    static constexpr bool move_constructible_to = (std::is_constructible_v<T, NormalizedType<SrcType> &&> || (std::is_constructible_v<Ts, NormalizedType<SrcType> &&> || ...));

    static constexpr bool copy_constructible = (std::is_copy_constructible_v<T> && (std::is_copy_constructible_v<Ts> && ...));
    static constexpr bool copy_assignable = (std::is_copy_assignable_v<T> && (std::is_copy_assignable_v<Ts> && ...));
    static constexpr bool move_constructible = (std::is_move_constructible_v<T> && (std::is_move_constructible_v<Ts> && ...));
    static constexpr bool move_assignable = (std::is_move_assignable_v<T> && (std::is_move_assignable_v<Ts> && ...));

    static inline bool CopyAssign(TypeID type_id, void *dst, const void *src)
    {
        if constexpr (std::is_copy_assignable_v<T>) {
            if (type_id == TypeID::ForType<NormalizedType<T>>()) {
                *static_cast<NormalizedType<T> *>(dst) = *static_cast<const NormalizedType<T> *>(src);

                return true;
            }
        }

        return VariantHelper<Ts...>::CopyAssign(type_id, dst, src);
    }

    static inline bool CopyConstruct(TypeID type_id, void *dst, const void *src)
    {
        if constexpr (std::is_copy_constructible_v<T>) {
            if (type_id == TypeID::ForType<NormalizedType<T>>()) {
                Memory::Construct<NormalizedType<T>>(dst, *static_cast<const NormalizedType<T> *>(src));

                return true;
            }
        }

        return VariantHelper<Ts...>::CopyConstruct(type_id, dst, src);
    }

    static inline void MoveAssign(TypeID type_id, void *dst, void *src)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            *static_cast<NormalizedType<T> *>(dst) = std::move(*static_cast<NormalizedType<T> *>(src));
        } else {
            VariantHelper<Ts...>::MoveAssign(type_id, dst, src);
        }
    }

    static inline bool MoveConstruct(TypeID type_id, void *dst, void *src)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            Memory::Construct<NormalizedType<T>>(dst, std::move(*static_cast<NormalizedType<T> *>(src)));

            return true;
        } else {
            return VariantHelper<Ts...>::MoveConstruct(type_id, dst, src);
        }
    }

    static inline void Destruct(TypeID type_id, void *data)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            Memory::Destruct<NormalizedType<T>>(data);
        } else {
            VariantHelper<Ts...>::Destruct(type_id, data);
        }
    }

    static inline bool Compare(TypeID type_id, const void *data, const void *other_data)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            return *static_cast<const NormalizedType<T> *>(data) == *static_cast<const NormalizedType<T> *>(other_data);
        } else {
            return VariantHelper<Ts...>::Compare(type_id, data, other_data);
        }
    }

    static inline HashCode GetHashCode(TypeID type_id, const void *data)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            return HashCode::GetHashCode(*static_cast<const NormalizedType<T> *>(data));
        } else {
            return VariantHelper<Ts...>::GetHashCode(type_id, data);
        }
    }
};

template <>
struct VariantHelper<>
{
    template <class SrcType>
    static constexpr bool holds_type = false;

    template <class SrcType>
    static constexpr bool copy_assignable_to = false;

    template <class SrcType>
    static constexpr bool move_assignable_to = false;

    template <class SrcType>
    static constexpr bool copy_constructible_to = false;

    template <class SrcType>
    static constexpr bool move_constructible_to = false;

    static inline bool CopyAssign(TypeID type_id, void *dst, const void *src) { return false; }
    static inline bool CopyConstruct(TypeID type_id, void *dst, const void *src) { return false; }
    static inline void MoveAssign(TypeID type_id, void *dst, void *src) {}
    static inline bool MoveConstruct(TypeID type_id, void *dst, void *src) { return false; }

    static inline void Destruct(TypeID type_id, void *data) {}
    static inline bool Destruct(TypeID type_id, void *data, const void *other_data) { return false; }

    static inline bool Compare(TypeID type_id, const void *data, const void *other_data) { return false; }

    static inline HashCode GetHashCode(TypeID type_id, const void *data) { return {}; }
};

template <class... Types>
class VariantBase
{
protected:
    using Helper = VariantHelper<Types...>;

    friend struct TypeIndexHelper<VariantBase<Types...>>;

public:
    static constexpr int invalid_type_index = -1;
    static constexpr TypeID type_ids[sizeof...(Types)] { TypeID::ForType<Types>()... };

    VariantBase()
        : m_current_index(-1)
    {
#ifdef HYP_DEBUG_MODE
        Memory::Garble(m_storage.GetPointer(), sizeof(m_storage));
#endif
    }

    VariantBase(const VariantBase &other)               = default;
    VariantBase &operator=(const VariantBase &other)    = default;

    VariantBase(VariantBase &&other) noexcept
        : VariantBase()
    {
        if (other.IsValid()) {
            if (Helper::MoveConstruct(other.CurrentTypeID(), m_storage.GetPointer(), other.m_storage.GetPointer())) {
                m_current_index = other.m_current_index;
            }
        }
    }

    VariantBase &operator=(VariantBase &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        if (IsValid()) {
            if (other.m_current_index == m_current_index) {
                Helper::MoveAssign(CurrentTypeID(), m_storage.GetPointer(), other.m_storage.GetPointer());
            } else {
                Helper::Destruct(CurrentTypeID(), m_storage.GetPointer());

                m_current_index = invalid_type_index;

                if (other.IsValid()) {
                    if (Helper::MoveConstruct(other.CurrentTypeID(), m_storage.GetPointer(), other.m_storage.GetPointer())) {
                        m_current_index = other.m_current_index;
                    }
                }
            }
        } else if (other.IsValid()) {
            if (Helper::MoveConstruct(other.CurrentTypeID(), m_storage.GetPointer(), other.m_storage.GetPointer())) {
                m_current_index = other.m_current_index;
            }
        }

        AssertThrow(m_current_index == other.m_current_index);

        other.m_current_index = invalid_type_index;

#ifdef HYP_DEBUG_MODE
        Memory::Garble(other.m_storage.GetPointer(), sizeof(other.m_storage));
#endif

        return *this;
    }

    template <class T, typename = typename std::enable_if_t<!std::is_base_of_v<VariantBase, T> && std::is_copy_constructible_v<T>>>
    explicit VariantBase(const T &value)
        : m_current_index(invalid_type_index)
    {
        static_assert(Helper::template holds_type<T> || resolution_failure<T>, "Type is not valid for the variant");

        const TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrow(Helper::CopyConstruct(type_id, m_storage.GetPointer(), std::addressof(value)));

        m_current_index = TypeIndexHelper<VariantBase<Types...>>{}(type_id);
    }

    template <class T, typename = typename std::enable_if_t<!std::is_base_of_v<VariantBase, T>>>
    explicit VariantBase(T &&value) noexcept
        : m_current_index(invalid_type_index)
    {
        static_assert(Helper::template holds_type<T> || resolution_failure<T>, "Type is not valid for the variant");

        const TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrow(Helper::MoveConstruct(type_id, m_storage.GetPointer(), std::addressof(value)));

        m_current_index = TypeIndexHelper<VariantBase<Types...>>{}(type_id);
    }

    ~VariantBase()
    {
        if (IsValid()) {
            Helper::Destruct(CurrentTypeID(), m_storage.GetPointer());
        }
    }
    
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return CurrentTypeID(); }
    
    HYP_FORCE_INLINE int GetTypeIndex() const
        { return m_current_index; }
    
    HYP_FORCE_INLINE void *GetPointer()
        { return m_storage.GetPointer(); }
    
    HYP_FORCE_INLINE const void *GetPointer() const
        { return m_storage.GetPointer(); }

    HYP_FORCE_INLINE bool operator==(const VariantBase &other) const
    {
        if (m_current_index != other.m_current_index) {
            return false;
        }

        if (!IsValid() && !other.IsValid()) {
            return true;
        }

        return Helper::Compare(CurrentTypeID(), m_storage.GetPointer(), other.m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE bool Is() const
        { return CurrentTypeID() == TypeID::ForType<NormalizedType<T>>(); }

    HYP_FORCE_INLINE bool IsValid() const
        { return m_current_index != invalid_type_index; }

    template <class T, class ReturnType = NormalizedType<T>>
    HYP_FORCE_INLINE bool Get(ReturnType *out_value) const
    {
        AssertThrow(out_value != nullptr);
        
        if (Is<ReturnType>()) {
            *out_value = *static_cast<const ReturnType *>(m_storage.GetPointer());

            return true;
        }

        return false;
    }

    template <class T>
    HYP_FORCE_INLINE T &Get()
    {
        AssertThrowMsg(Is<NormalizedType<T>>(), "Held type differs from requested type!");

        return *static_cast<NormalizedType<T> *>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE const T &Get() const
    {
        AssertThrowMsg(Is<NormalizedType<T>>(), "Held type differs from requested type!");

        return *static_cast<const NormalizedType<T> *>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE T &GetUnchecked()
    {
        return *static_cast<NormalizedType<T> *>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE const T &GetUnchecked() const
    {
        return *static_cast<const NormalizedType<T> *>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE T *TryGet()
    {
        if (!Is<T>()) {
            return nullptr;
        }

        return static_cast<T *>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE const T *TryGet() const
    {
        if (!Is<T>()) {
            return nullptr;
        }

        return static_cast<const T *>(m_storage.GetPointer());
    }

    template <class T, typename = std::enable_if_t<std::is_copy_constructible_v<T>>>
    void Set(const T &value)
    {
        static_assert(Helper::template holds_type<T>, "Type is not valid for the variant");

        if (IsValid()) {
            Helper::Destruct(CurrentTypeID(), m_storage.GetPointer());
        }

        m_current_index = invalid_type_index;

        const TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrowMsg(
            Helper::CopyConstruct(type_id, m_storage.GetPointer(), &value),
            "Not a valid type for the Variant"
        );

        m_current_index = TypeIndexHelper<VariantBase<Types...>>{}(type_id);
    }

    template <class T>
    void Set(T &&value)
    {
        static_assert(Helper::template holds_type<T>, "Type is not valid for the variant");

        if (IsValid()) {
            Helper::Destruct(CurrentTypeID(), m_storage.GetPointer());
        }

        m_current_index = invalid_type_index;

        const TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrowMsg(
            Helper::MoveConstruct(type_id, m_storage.GetPointer(), &value),
            "Not a valid type for the Variant"
        );

        m_current_index = TypeIndexHelper<VariantBase<Types...>>{}(type_id);
    }

    template <class T, class... Args>
    void Emplace(Args &&... args)
    {
        static_assert(Helper::template holds_type<T>, "Type is not valid for the variant");

        if (IsValid()) {
            Helper::Destruct(CurrentTypeID(), m_storage.GetPointer());
        }

        m_current_index = invalid_type_index;

        const TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        Memory::Construct<NormalizedType<T>>(m_storage.GetPointer(), std::forward<Args>(args)...);

        m_current_index = TypeIndexHelper<VariantBase<Types...>>{}(type_id);
    }

    /*! \brief Resets the Variant into an invalid state.
     * If there is any present value, it will be destructed
     */
    void Reset()
    {
        if (IsValid()) {
            Helper::Destruct(CurrentTypeID(), m_storage.GetPointer());
        }

        m_current_index = invalid_type_index;
    }

    HashCode GetHashCode() const
    {
        return Helper::GetHashCode(CurrentTypeID(), m_storage.GetPointer());
    }

protected:
    static constexpr SizeType max_size = MathUtil::Max(sizeof(Types)...);
    static constexpr SizeType max_align = MathUtil::Max(alignof(Types)...);

    int m_current_index;

    struct alignas(max_align) Storage
    {
        alignas(max_align) ubyte data_buffer[max_size];

        void *GetPointer() { return static_cast<void *>(&data_buffer[0]); }
        const void *GetPointer() const { return static_cast<const void *>(&data_buffer[0]); }
    } m_storage;

    HYP_FORCE_INLINE constexpr TypeID CurrentTypeID() const
        { return type_ids[m_current_index]; }
};

template <bool IsCopyable, class... Types>
struct Variant : public VariantBase<Types...> { };

template <class... Types>
struct Variant<true, Types...> : public VariantBase<Types...>
{
private:
    using Base = VariantBase<Types...>;

public:
    Variant() = default;
    Variant(Variant &&other) noexcept
        : Base(std::move(other))
    {
    }

    Variant &operator=(Variant &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    Variant(const Variant &other)
        : Base()
    {
        if (other.IsValid()) {
            AssertThrowMsg(Base::Helper::CopyConstruct(other.CurrentTypeID(), Base::m_storage.GetPointer(), other.m_storage.GetPointer()),
                "Variant types not compatible");

            Base::m_current_index = other.m_current_index;
        }
    }

    Variant &operator=(const Variant &other)
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        if (Base::IsValid()) {
            if (other.m_current_index == Base::m_current_index) {
                AssertThrowMsg(Base::Helper::CopyAssign(Base::CurrentTypeID(), Base::m_storage.GetPointer(), other.m_storage.GetPointer()),
                    "Variant types not compatible");
            } else {
                Base::Helper::Destruct(Base::CurrentTypeID(), Base::m_storage.GetPointer());

                Base::m_current_index = Base::invalid_type_index;

                if (other.IsValid()) {
                    AssertThrowMsg(Base::Helper::CopyConstruct(other.CurrentTypeID(), Base::m_storage.GetPointer(), other.m_storage.GetPointer()),
                        "Variant types not compatible");

                    Base::m_current_index = other.m_current_index;
                }
            }
        } else if (other.IsValid()) {
            AssertThrowMsg(Base::Helper::CopyConstruct(other.CurrentTypeID(), Base::m_storage.GetPointer(), other.m_storage.GetPointer()),
                "Variant types not compatible");

            Base::m_current_index = other.m_current_index;
        }

        AssertThrow(Base::m_current_index == other.m_current_index);

        return *this;
    }

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant> && std::is_copy_constructible_v<T>>>
    explicit Variant(const T &value)
        : Base(value)
    {
    }

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant>>>
    explicit Variant(T &&value) noexcept
        : Base(std::forward<T>(value))
    {
    }

    ~Variant() = default;
};

template <class... Types>
struct Variant<false, Types...> : public VariantBase<Types...>
{
private:
    using Base = VariantBase<Types...>;

public:
    Variant() = default;
    Variant(Variant &&other) noexcept
        : Base(std::move(other))
    {
    }

    Variant &operator=(Variant &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    Variant(const Variant &other)               = delete;
    Variant &operator=(const Variant &other)    = delete;

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant> && std::is_copy_constructible_v<T>>>
    explicit Variant(const T &value)
        : Base(value)
    {
    }

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant>>>
    explicit Variant(T &&value) noexcept
        : Base(std::forward<T>(value))
    {
    }

    ~Variant() = default;
};

} // namespace detail

template <class... Types>
struct Variant;

template <class... Types, class FunctionType>
static inline void Visit(const Variant<Types...> &variant, FunctionType &&fn);

template <class... Types>
struct Variant
    : private ConstructAssignmentTraits<
        true,
        utilities::detail::VariantHelper<Types...>::copy_constructible,
        utilities::detail::VariantHelper<Types...>::move_constructible,
        Variant<Types...>
      >
{
    static constexpr int invalid_type_index = utilities::detail::VariantBase<Types...>::invalid_type_index;
    static constexpr TypeID type_ids[sizeof...(Types)] { TypeID::ForType<Types>()... };

    // template <class ...Args>
    // Variant(Args &&... args)
    //     : m_holder(std::forward<Args>(args)...)
    // {
    // }

    Variant()                                       = default;
    Variant(const Variant &other)                   = default;
    Variant &operator=(const Variant &other)        = default;
    Variant(Variant &&other) noexcept               = default;
    Variant &operator=(Variant &&other) noexcept    = default;
    ~Variant()                                      = default;

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant> && std::is_copy_constructible_v<T>>>
    Variant(const T &value)
        : m_holder(value)
    {
    }

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant>>>
    Variant(T &&value) noexcept
        : m_holder(std::forward<T>(value))
    {
    }
    
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_holder.GetTypeID(); }
    
    HYP_FORCE_INLINE int GetTypeIndex() const
        { return m_holder.GetTypeIndex(); }
    
    HYP_FORCE_INLINE bool operator==(const Variant &other) const
        { return m_holder == other.m_holder; }
    
    HYP_FORCE_INLINE bool operator!=(const Variant &other) const
        { return !(m_holder == other.m_holder); }

    HYP_FORCE_INLINE explicit operator bool() const
        { return m_holder.IsValid(); }

    template <class T>
    HYP_FORCE_INLINE bool Is() const
        { return m_holder.template Is<T>(); }

    HYP_FORCE_INLINE bool IsValid() const
        { return m_holder.IsValid(); }

    HYP_FORCE_INLINE bool HasValue() const
        { return m_holder.IsValid(); }

    HYP_FORCE_INLINE void *GetPointer() &
        { return m_holder.GetPointer(); }

    HYP_FORCE_INLINE const void *GetPointer() const &
        { return m_holder.GetPointer(); }

    template <class T, class ReturnType = NormalizedType<T>>
    HYP_FORCE_INLINE bool Get(ReturnType *out_value) const
        { return m_holder.template Get<T, ReturnType>(out_value); }

    template <class T>
    HYP_FORCE_INLINE T &Get() &
        { return m_holder.template Get<T>(); }

    template <class T>
    HYP_FORCE_INLINE const T &Get() const &
        { return m_holder.template Get<T>(); }

    template <class T>
    HYP_FORCE_INLINE T &GetUnchecked() &
        { return m_holder.template GetUnchecked<T>(); }

    template <class T>
    HYP_FORCE_INLINE const T &GetUnchecked() const &
        { return m_holder.template GetUnchecked<T>(); }

    template <class T>
    HYP_FORCE_INLINE T *TryGet() &
        { return m_holder.template TryGet<T>(); }

    template <class T>
    HYP_FORCE_INLINE const T *TryGet() const &
        { return m_holder.template TryGet<T>(); }

    template <class T>
    HYP_FORCE_INLINE T &TryGet(T &default_value) &
    {
        if (Is<T>()) {
            return Get<T>();
        }

        return default_value;
    }

    template <class T>
    HYP_FORCE_INLINE const T &TryGet(const T &default_value) const &
    {
        if (Is<T>()) {
            return Get<T>();
        }

        return default_value;
    }

    template <class T>
    HYP_FORCE_INLINE T TryGet(T &&default_value) const
    {
        if (Is<T>()) {
            return Get<T>();
        }

        return std::forward<T>(default_value);
    }

    template <class T>
    HYP_FORCE_INLINE T TryGet(const T &default_value) &&
    {
        if (Is<T>()) {
            return Get<T>();
        }

        return default_value;
    }

    template <class T, typename = std::enable_if_t<std::is_copy_constructible_v<T>>>
    HYP_FORCE_INLINE void Set(const T &value)
        { m_holder.template Set<T>(value); }

    template <class T>
    HYP_FORCE_INLINE void Set(T &&value)
        { return m_holder.template Set<T>(std::forward<T>(value)); }

    template <class T, class... Args>
    HYP_FORCE_INLINE void Emplace(Args &&... args)
        { return m_holder.template Emplace<T>(std::forward<Args>(args)...); }
    

    /*! \brief Resets the Variant into an invalid state.
     * If there is any present value, it will be destructed
     */
    HYP_FORCE_INLINE void Reset()
        { m_holder.Reset(); }

    HYP_FORCE_INLINE AnyRef ToRef()
    {
        if (!IsValid()) {
            return AnyRef();
        }

        return AnyRef(m_holder.GetPointer(), m_holder.GetTypeID());
    }

    HYP_FORCE_INLINE ConstAnyRef ToRef() const
    {
        if (!IsValid()) {
            return ConstAnyRef();
        }

        return ConstAnyRef(m_holder.GetPointer(), m_holder.GetTypeID());
    }
    
    HYP_FORCE_INLINE HashCode GetHashCode() const
        { return m_holder.GetHashCode(); }

    template <class FunctionType>
    HYP_FORCE_INLINE void Visit(FunctionType &&fn) const
    {
        ::hyperion::utilities::Visit(*this, std::forward<FunctionType>(fn));
    }

private:
    utilities::detail::Variant<
        utilities::detail::VariantHelper<Types...>::copy_constructible,
        Types...
    > m_holder;
};

namespace detail {

#pragma region TypeIndex

template <class T>
struct TypeIndex_Impl
{
    constexpr bool operator()(TypeID type_id, int &index) const
    {
        constexpr TypeID other_type_id = TypeID::ForType<T>();

        if (type_id != other_type_id) {
            ++index;

            return false;
        }

        return true;
    }
};

template <class VariantType>
struct TypeIndexHelper;

template <class T>
struct TypeIndexHelper<VariantBase<T>>
{
    constexpr int operator()(TypeID type_id) const
    {
        int index = 0;
        
        if (!TypeIndex_Impl<T>{}(type_id, index)) {
            return -1;
        }

        return index;
    }
};

template <class T, class... Types>
struct TypeIndexHelper<VariantBase<T, Types...>>
{
    constexpr int operator()(TypeID type_id) const
    {
        int value = 0;
        
        if (!(TypeIndex_Impl<T>{}(type_id, value) || (TypeIndex_Impl<Types>{}(type_id, value) || ...))) {
            return -1;
        }

        return value;
    }
};

#pragma endregion TypeIndex

#pragma region VisitHelper

// template <class T, class FunctionType>
// struct Visit_Impl
// {
//     template <class... Types>
//     constexpr bool operator()(const utilities::Variant<Types...> &variant, const FunctionType &fn) const
//     {
//         if (!variant.template Is<T>()) {
//             return false;
//         }

//         fn(variant.template GetUnchecked<T>());

//         return true;
//     }
// };

template <class VariantType, class FunctionType, class T>
static inline void Variant_InvokeFunction(const VariantType &variant, FunctionType &fn)
{
    fn(variant.template GetUnchecked<T>());
}

template <class VariantType>
struct VisitHelper;

template <class... Types>
struct VisitHelper<utilities::Variant<Types...>>
{
    template <class FunctionType>
    void operator()(const utilities::Variant<Types...> &variant, FunctionType &&fn) const
    {
        using InvokeFunctionWrapper = std::add_pointer_t<void(const utilities::Variant<Types...> &, FunctionType &)>;
        
        static const InvokeFunctionWrapper invoke_fns[sizeof...(Types)] = {
            &Variant_InvokeFunction<utilities::Variant<Types...>, FunctionType, Types>...
        };

        if (!variant.IsValid()) {
            return;
        }

        const int type_index = variant.GetTypeIndex();

#ifdef HYP_DEBUG_MODE
        // Sanity check
        AssertThrow(type_index < sizeof...(Types));
#endif

        invoke_fns[type_index](variant, fn);
    }
};

#pragma endregion VisitHelper

} // namespace detail

template <class... Types, class FunctionType>
static inline void Visit(const Variant<Types...> &variant, FunctionType &&fn)
{
    detail::VisitHelper<Variant<Types...>>{}(variant, std::forward<FunctionType>(fn));
}

} // namespace utilities

template <class... Types>
using Variant = utilities::Variant<Types...>;

using utilities::Visit;

} // namespace hyperion

#endif