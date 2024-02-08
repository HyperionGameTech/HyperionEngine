#ifndef HYPERION_V2_LIB_VARIANT_H
#define HYPERION_V2_LIB_VARIANT_H

#include <core/lib/CMemory.hpp>
#include <core/lib/TypeID.hpp>
#include <core/lib/Traits.hpp>
#include <Constants.hpp>

#include <math/MathUtil.hpp>

#include <system/Debug.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

namespace containers {
namespace detail {

template <class ... Ts>
struct VariantHelper;

template <class T, class ... Ts>
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
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            *static_cast<NormalizedType<T> *>(dst) = *static_cast<const NormalizedType<T> *>(src);

            return true;
        } else {
            return VariantHelper<Ts...>::CopyAssign(type_id, dst, src);
        }
    }

    static inline bool CopyConstruct(TypeID type_id, void *dst, const void *src)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            Memory::Construct<NormalizedType<T>>(dst, *static_cast<const NormalizedType<T> *>(src));

            return true;
        } else {
            return VariantHelper<Ts...>::CopyConstruct(type_id, dst, src);
        }
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

template <class ...Types>
class VariantBase
{
protected:
    using Helper = VariantHelper<Types...>;

    static const inline TypeID invalid_type_id = TypeID::ForType<void>();

public:
    VariantBase()
        : m_current_type_id(invalid_type_id)
    {
#ifdef HYP_DEBUG_MODE
        Memory::Garble(m_storage.GetPointer(), sizeof(m_storage));
#endif
    }

    VariantBase(const VariantBase &other) = default;
    VariantBase &operator=(const VariantBase &other) = default;

    VariantBase(VariantBase &&other) noexcept
        : VariantBase()
    {
        if (other.IsValid()) {
            if (Helper::MoveConstruct(other.m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer())) {
                m_current_type_id = other.m_current_type_id;
            }
        }
    }

    VariantBase &operator=(VariantBase &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        if (IsValid()) {
            if (other.m_current_type_id == m_current_type_id) {
                Helper::MoveAssign(m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer());
            } else {
                Helper::Destruct(m_current_type_id, m_storage.GetPointer());

                m_current_type_id = invalid_type_id;

                if (other.IsValid()) {
                    if (Helper::MoveConstruct(other.m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer())) {
                        m_current_type_id = other.m_current_type_id;
                    }
                }
            }
        } else if (other.IsValid()) {
            if (Helper::MoveConstruct(other.m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer())) {
                m_current_type_id = other.m_current_type_id;
            }
        }

        AssertThrow(m_current_type_id == other.m_current_type_id);

        other.m_current_type_id = invalid_type_id;

#ifdef HYP_DEBUG_MODE
        Memory::Garble(other.m_storage.GetPointer(), sizeof(other.m_storage));
#endif

        return *this;
    }

    template <class T, typename = typename std::enable_if_t<!std::is_base_of_v<VariantBase, T>>>
    explicit VariantBase(const T &value)
        : m_current_type_id(invalid_type_id)
    {
        static_assert(Helper::template holds_type<T>, "Type is not valid for the variant");

        const TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrow(Helper::CopyConstruct(type_id, m_storage.GetPointer(), &value));
        m_current_type_id = type_id;
    }

    template <class T, typename = typename std::enable_if_t<!std::is_base_of_v<VariantBase, T>>>
    explicit VariantBase(T &&value) noexcept
        : m_current_type_id(invalid_type_id)
    {
        static_assert(Helper::template holds_type<T>, "Type is not valid for the variant");

        const TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrow(Helper::MoveConstruct(type_id, m_storage.GetPointer(), &value));
        m_current_type_id = type_id;
    }

    ~VariantBase()
    {
        if (IsValid()) {
            Helper::Destruct(m_current_type_id, m_storage.GetPointer());
        }
    }
    
    HYP_FORCE_INLINE
    TypeID GetTypeID() const
        { return m_current_type_id; }
    
    HYP_FORCE_INLINE
    void *GetPointer()
        { return m_storage.GetPointer(); }
    
    HYP_FORCE_INLINE
    const void *GetPointer() const
        { return m_storage.GetPointer(); }

    bool operator==(const VariantBase &other) const
    {
        if (m_current_type_id != other.m_current_type_id) {
            return false;
        }

        if (!IsValid() && !other.IsValid()) {
            return true;
        }

        return Helper::Compare(m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE
    bool Is() const
        { return m_current_type_id == TypeID::ForType<NormalizedType<T>>(); }

    HYP_FORCE_INLINE
    bool IsValid() const
        { return m_current_type_id != invalid_type_id; }

    template <class T, class ReturnType = NormalizedType<T>>
    HYP_FORCE_INLINE
    bool Get(ReturnType *out_value) const
    {
        AssertThrow(out_value != nullptr);
        
        if (Is<ReturnType>()) {
            *out_value = *static_cast<const ReturnType *>(m_storage.GetPointer());

            return true;
        }

        return false;
    }

    template <class T>
    HYP_FORCE_INLINE
    T &Get()
    {
        AssertThrowMsg(Is<NormalizedType<T>>(), "Held type differs from requested type!");

        return *static_cast<NormalizedType<T> *>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE
    const T &Get() const
    {
        AssertThrowMsg(Is<NormalizedType<T>>(), "Held type differs from requested type!");

        return *static_cast<const NormalizedType<T> *>(m_storage.GetPointer());
    }

    template <class T>
    T *TryGet()
    {
        if (!Is<T>()) {
            return nullptr;
        }

        return static_cast<T *>(m_storage.GetPointer());
    }

    template <class T>
    const T *TryGet() const
    {
        if (!Is<T>()) {
            return nullptr;
        }

        return static_cast<const T *>(m_storage.GetPointer());
    }

    template <class T>
    void Set(const T &value)
    {
        static_assert(Helper::template holds_type<T>, "Type is not valid for the variant");

        if (IsValid()) {
            Helper::Destruct(m_current_type_id, m_storage.GetPointer());
        }

        m_current_type_id = invalid_type_id;

        const auto type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrowMsg(
            Helper::CopyConstruct(type_id, m_storage.GetPointer(), &value),
            "Not a valid type for the Variant"
        );

        m_current_type_id = type_id;
    }

    template <class T>
    void Set(T &&value)
    {
        static_assert(Helper::template holds_type<T>, "Type is not valid for the variant");

        if (IsValid()) {
            Helper::Destruct(m_current_type_id, m_storage.GetPointer());
        }

        m_current_type_id = invalid_type_id;

        const auto type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrowMsg(
            Helper::MoveConstruct(type_id, m_storage.GetPointer(), &value),
            "Not a valid type for the Variant"
        );

        m_current_type_id = type_id;
    }

    /*! \brief Resets the Variant into an invalid state.
     * If there is any present value, it will be destructed
     */
    void Reset()
    {
        if (IsValid()) {
            Helper::Destruct(m_current_type_id, m_storage.GetPointer());
        }

        m_current_type_id = invalid_type_id;
    }

    HashCode GetHashCode() const
    {
        return Helper::GetHashCode(m_current_type_id, m_storage.GetPointer());
    }

protected:
    static constexpr SizeType max_size = MathUtil::Max(sizeof(Types)...);
    static constexpr SizeType max_align = MathUtil::Max(alignof(Types)...);

    TypeID m_current_type_id;

    struct alignas(max_align) Storage
    {
        alignas(max_align) UByte data_buffer[max_size];

        void *GetPointer() { return static_cast<void *>(&data_buffer[0]); }
        const void *GetPointer() const { return static_cast<const void *>(&data_buffer[0]); }
    } m_storage;
};

template <bool IsCopyable, class ...Types>
struct Variant : public VariantBase<Types...> { };

template <class ...Types>
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
            AssertThrowMsg(Base::Helper::CopyConstruct(other.m_current_type_id, Base::m_storage.GetPointer(), other.m_storage.GetPointer()),
                "Variant types not compatible");

            Base::m_current_type_id = other.m_current_type_id;
        }
    }

    Variant &operator=(const Variant &other)
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        if (Base::IsValid()) {
            if (other.m_current_type_id == Base::m_current_type_id) {
                AssertThrowMsg(Base::Helper::CopyAssign(Base::m_current_type_id, Base::m_storage.GetPointer(), other.m_storage.GetPointer()),
                    "Variant types not compatible");
            } else {
                Base::Helper::Destruct(Base::m_current_type_id, Base::m_storage.GetPointer());

                Base::m_current_type_id = Base::invalid_type_id;

                if (other.IsValid()) {
                    AssertThrowMsg(Base::Helper::CopyConstruct(other.m_current_type_id, Base::m_storage.GetPointer(), other.m_storage.GetPointer()),
                        "Variant types not compatible");

                    Base::m_current_type_id = other.m_current_type_id;
                }
            }
        } else if (other.IsValid()) {
            AssertThrowMsg(Base::Helper::CopyConstruct(other.m_current_type_id, Base::m_storage.GetPointer(), other.m_storage.GetPointer()),
                "Variant types not compatible");

            Base::m_current_type_id = other.m_current_type_id;
        }

        AssertThrow(Base::m_current_type_id == other.m_current_type_id);

        return *this;
    }

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant>>>
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

template <class ...Types>
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

    Variant(const Variant &other) = delete;
    Variant &operator=(const Variant &other) = delete;

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant>>>
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
} // namespace containers

template <class ...Types>
struct Variant
    : private ConstructAssignmentTraits<
        containers::detail::VariantHelper<Types...>::copy_constructible,
        containers::detail::VariantHelper<Types...>::move_constructible,
        Variant<Types...>
      >
{
    // template <class ...Args>
    // Variant(Args &&... args)
    //     : m_holder(std::forward<Args>(args)...)
    // {
    // }

    Variant() = default;
    Variant(const Variant &other) = default;
    Variant &operator=(const Variant &other) = default;
    Variant(Variant &&other) noexcept = default;
    Variant &operator=(Variant &&other) noexcept = default;
    ~Variant() = default;

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant>>>
    Variant(const T &value)
        : m_holder(value)
    {
    }

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant>>>
    Variant(T &&value) noexcept
        : m_holder(std::forward<T>(value))
    {
    }
    
    HYP_FORCE_INLINE
    TypeID GetTypeID() const
        { return m_holder.GetTypeID(); }
    
    HYP_FORCE_INLINE
    bool operator==(const Variant &other) const
        { return m_holder == other.m_holder; }
    
    HYP_FORCE_INLINE
    bool operator!=(const Variant &other) const
        { return !(m_holder == other.m_holder); }

    HYP_FORCE_INLINE
    explicit operator bool() const
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
    HYP_FORCE_INLINE
    bool Get(ReturnType *out_value) const
        { return m_holder.template Get<T, ReturnType>(out_value); }

    template <class T>
    HYP_FORCE_INLINE
    T &Get() &
        { return m_holder.template Get<T>(); }

    template <class T>
    HYP_FORCE_INLINE
    const T &Get() const &
        { return m_holder.template Get<T>(); }

    template <class T>
    HYP_FORCE_INLINE
    T *TryGet() &
        { return m_holder.template TryGet<T>(); }

    template <class T>
    HYP_FORCE_INLINE
    const T *TryGet() const &
        { return m_holder.template TryGet<T>(); }

    template <class T>
    HYP_FORCE_INLINE
    void Set(const T &value)
        { m_holder.template Set<T>(value); }

    template <class T>
    HYP_FORCE_INLINE
    void Set(T &&value)
        { return m_holder.template Set<T>(std::forward<T>(value)); }
    

    /*! \brief Resets the Variant into an invalid state.
     * If there is any present value, it will be destructed
     */
    HYP_FORCE_INLINE
    void Reset()
        { m_holder.Reset(); }
    
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
        { return m_holder.GetHashCode(); }

private:
    containers::detail::Variant<
        containers::detail::VariantHelper<Types...>::copy_constructible,
        Types...
    > m_holder;
};

} // namespace hyperion

#endif