/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/Memory.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/utilities/TypeId.hpp>

#include <core/debug/Debug.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Traits.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

namespace hyperion {

namespace utilities {
template <class VariantType>
struct TypeIndexHelper;

template <class... Ts>
struct VariantHelper;

template <class T, class... Ts>
struct VariantHelper<T, Ts...>
{
    template <class SrcType>
    static constexpr bool holdsType = std::is_same_v<T, NormalizedType<SrcType>> || (std::is_same_v<Ts, NormalizedType<SrcType>> || ...);

    static constexpr bool copyConstructible = (std::is_copy_constructible_v<T> && (std::is_copy_constructible_v<Ts> && ...));
    static constexpr bool copyAssignable = (std::is_copy_assignable_v<T> && (std::is_copy_assignable_v<Ts> && ...));
    static constexpr bool moveConstructible = (std::is_move_constructible_v<T> && (std::is_move_constructible_v<Ts> && ...));
    static constexpr bool moveAssignable = (std::is_move_assignable_v<T> && (std::is_move_assignable_v<Ts> && ...));
    
    static constexpr bool triviallyDestructible = (std::is_trivially_destructible_v<T> && (std::is_trivially_destructible_v<Ts> && ...));

    static constexpr TypeId thisTypeId = TypeId::ForType<NormalizedType<T>>();

    static inline bool CopyAssign(TypeId typeId, void* dst, const void* src)
    {
        HYP_CORE_ASSERT(typeId == thisTypeId);

        if constexpr (std::is_copy_assignable_v<T>)
        {
            *static_cast<NormalizedType<T>*>(dst) = *static_cast<const NormalizedType<T>*>(src);

            return true;
        }

        return false;
    }

    static inline bool CopyConstruct(TypeId typeId, void* dst, const void* src)
    {
        HYP_CORE_ASSERT(typeId == thisTypeId);

        if constexpr (std::is_copy_constructible_v<T>)
        {
            Memory::Construct<NormalizedType<T>>(dst, *static_cast<const NormalizedType<T>*>(src));

            return true;
        }

        return false;
    }

    static inline bool MoveAssign(TypeId typeId, void* dst, void* src)
    {
        HYP_CORE_ASSERT(typeId == thisTypeId);

        *static_cast<NormalizedType<T>*>(dst) = std::move(*static_cast<NormalizedType<T>*>(src));
        
        return true;
    }

    static inline bool MoveConstruct(TypeId typeId, void* dst, void* src)
    {
        HYP_CORE_ASSERT(typeId == thisTypeId);

        Memory::Construct<NormalizedType<T>>(dst, std::move(*static_cast<NormalizedType<T>*>(src)));

        return true;
    }

    static inline void Destruct(TypeId typeId, void* data)
    {
        HYP_CORE_ASSERT(typeId == thisTypeId);

        Memory::Destruct<NormalizedType<T>>(data);
    }

    static inline bool Compare(TypeId typeId, const void* data, const void* otherData)
    {
        if (typeId == thisTypeId)
        {
            return *static_cast<const NormalizedType<T>*>(data) == *static_cast<const NormalizedType<T>*>(otherData);
        }

        return VariantHelper<Ts...>::Compare(typeId, data, otherData);
    }

    static inline HashCode GetHashCode(TypeId typeId, const void* data)
    {
        if (typeId == thisTypeId)
        {
            return HashCode::GetHashCode(*static_cast<const NormalizedType<T>*>(data));
        }

        return VariantHelper<Ts...>::GetHashCode(typeId, data);
    }
};

template <>
struct VariantHelper<>
{
    template <class SrcType>
    static constexpr bool holdsType = false;

    static inline bool CopyAssign(TypeId typeId, void* dst, const void* src)
    {
        return false;
    }

    static inline bool CopyConstruct(TypeId typeId, void* dst, const void* src)
    {
        return false;
    }

    static inline bool MoveAssign(TypeId typeId, void* dst, void* src)
    {
        return false;
    }

    static inline bool MoveConstruct(TypeId typeId, void* dst, void* src)
    {
        return false;
    }

    static inline void Destruct(TypeId typeId, void* data)
    {
    }

    static inline bool Compare(TypeId typeId, const void* data, const void* otherData)
    {
        return false;
    }

    static inline HashCode GetHashCode(TypeId typeId, const void* data)
    {
        return {};
    }
};

template <class... Types>
class VariantBase
{
protected:
    using Helper = VariantHelper<Types...>;

    friend struct TypeIndexHelper<VariantBase<Types...>>;

    using CopyAssignFunction = bool (*)(TypeId typeId, void* dst, const void* src);
    using CopyConstructFunction = bool (*)(TypeId typeId, void* dst, const void* src);
    using MoveAssignFunction = bool (*)(TypeId typeId, void* dst, void* src);
    using MoveConstructFunction = bool (*)(TypeId typeId, void* dst, void* src);
    using DestructFunction = void (*)(TypeId typeId, void* data);

    static const inline CopyAssignFunction copyAssignFunctions[] = { &VariantHelper<Types>::CopyAssign... };
    static const inline CopyConstructFunction copyConstructFunctions[] = { &VariantHelper<Types>::CopyConstruct... };
    static const inline MoveAssignFunction moveAssignFunctions[] = { &VariantHelper<Types>::MoveAssign... };
    static const inline MoveConstructFunction moveConstructFunctions[] = { &VariantHelper<Types>::MoveConstruct... };
    static const inline DestructFunction destructFunctions[] = { &VariantHelper<Types>::Destruct... };

public:
    static constexpr int invalidTypeIndex = -1;
    static constexpr TypeId typeIds[sizeof...(Types) + 1] { TypeId::Void(), TypeId::ForType<Types>()... };

    VariantBase()
        : m_currentIndex(-1)
    {
#ifdef HYP_DEBUG_MODE
        Memory::Garble(m_storage.GetPointer(), sizeof(m_storage));
#endif
    }

    VariantBase(const VariantBase& other) = default;
    VariantBase& operator=(const VariantBase& other) = default;

    VariantBase(VariantBase&& other) noexcept
        : VariantBase()
    {
        if (other.IsValid())
        {
            if (moveConstructFunctions[other.m_currentIndex](other.CurrentTypeId(), m_storage.GetPointer(), other.m_storage.GetPointer()))
            {
                m_currentIndex = other.m_currentIndex;
            }
        }
    }

    VariantBase& operator=(VariantBase&& other) noexcept
    {
        if (std::addressof(other) == this)
        {
            return *this;
        }

        if (IsValid())
        {
            if (other.m_currentIndex == m_currentIndex)
            {
                moveAssignFunctions[m_currentIndex](CurrentTypeId(), m_storage.GetPointer(), other.m_storage.GetPointer());
            }
            else
            {
                if (!VariantHelper<Types...>::triviallyDestructible)
                {
                    destructFunctions[m_currentIndex](CurrentTypeId(), m_storage.GetPointer());
                }

                m_currentIndex = invalidTypeIndex;

                if (other.IsValid())
                {
                    if (moveConstructFunctions[other.m_currentIndex](other.CurrentTypeId(), m_storage.GetPointer(), other.m_storage.GetPointer()))
                    {
                        m_currentIndex = other.m_currentIndex;
                    }
                }
            }
        }
        else if (other.IsValid())
        {
            if (moveConstructFunctions[other.m_currentIndex](other.CurrentTypeId(), m_storage.GetPointer(), other.m_storage.GetPointer()))
            {
                m_currentIndex = other.m_currentIndex;
            }
        }

        HYP_CORE_ASSERT(m_currentIndex == other.m_currentIndex);

        other.m_currentIndex = invalidTypeIndex;

#ifdef HYP_DEBUG_MODE
        Memory::Garble(other.m_storage.GetPointer(), sizeof(other.m_storage));
#endif

        return *this;
    }

    template <class T, typename = typename std::enable_if_t<!std::is_base_of_v<VariantBase, T> && std::is_copy_constructible_v<T>>>
    explicit VariantBase(const T& value)
        : m_currentIndex(invalidTypeIndex)
    {
        static_assert(Helper::template holdsType<T> || resolutionFailure<T>, "Type is not valid for the variant");

        constexpr TypeId typeId = TypeId::ForType<NormalizedType<T>>();

        m_currentIndex = TypeIndexHelper<VariantBase<Types...>> {}(typeId);
        const bool constructResult = copyConstructFunctions[m_currentIndex](typeId, m_storage.GetPointer(), std::addressof(value));

        HYP_CORE_ASSERT(constructResult);
    }

    template <class T, typename = typename std::enable_if_t<!std::is_base_of_v<VariantBase, T>>>
    explicit VariantBase(T&& value) noexcept
        : m_currentIndex(invalidTypeIndex)
    {
        static_assert(Helper::template holdsType<T> || resolutionFailure<T>, "Type is not valid for the variant");

        constexpr TypeId typeId = TypeId::ForType<NormalizedType<T>>();

        m_currentIndex = TypeIndexHelper<VariantBase<Types...>> {}(typeId);
        const bool constructResult = moveConstructFunctions[m_currentIndex](typeId, m_storage.GetPointer(), std::addressof(value));

        HYP_CORE_ASSERT(constructResult);
    }

    ~VariantBase()
    {
        if constexpr (!VariantHelper<Types...>::triviallyDestructible)
        {
            if (IsValid())
            {
                destructFunctions[m_currentIndex](CurrentTypeId(), m_storage.GetPointer());
            }
        }
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return CurrentTypeId();
    }

    HYP_FORCE_INLINE int GetTypeIndex() const
    {
        return m_currentIndex;
    }

    HYP_FORCE_INLINE void* GetPointer()
    {
        return m_storage.GetPointer();
    }

    HYP_FORCE_INLINE const void* GetPointer() const
    {
        return m_storage.GetPointer();
    }

    HYP_FORCE_INLINE bool operator==(const VariantBase& other) const
    {
        if (m_currentIndex != other.m_currentIndex)
        {
            return false;
        }
        
        if (!IsValid())
        {
            return false;
        }

        return VariantHelper<Types...>::Compare(CurrentTypeId(), m_storage.GetPointer(), other.m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeId otherTypeId = TypeId::ForType<NormalizedType<T>>();

        return CurrentTypeId() == otherTypeId;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_currentIndex != invalidTypeIndex;
    }

    template <class T, class ReturnType = NormalizedType<T>>
    HYP_FORCE_INLINE bool Get(ReturnType* outValue) const
    {
        HYP_CORE_ASSERT(outValue != nullptr);

        if (Is<ReturnType>())
        {
            *outValue = *static_cast<const ReturnType*>(m_storage.GetPointer());

            return true;
        }

        return false;
    }

    template <class T>
    HYP_FORCE_INLINE T& Get() &
    {
        HYP_CORE_ASSERT(Is<NormalizedType<T>>());

        return *static_cast<NormalizedType<T>*>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE const T& Get() const&
    {
        HYP_CORE_ASSERT(Is<NormalizedType<T>>());

        return *static_cast<const NormalizedType<T>*>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE T Get() &&
    {
        HYP_CORE_ASSERT(Is<NormalizedType<T>>());

        return std::move(*static_cast<NormalizedType<T>*>(m_storage.GetPointer()));
    }

    template <class T>
    HYP_FORCE_INLINE T& GetUnchecked() &
    {
        return *static_cast<NormalizedType<T>*>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE const T& GetUnchecked() const&
    {
        return *static_cast<const NormalizedType<T>*>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE T GetUnchecked() &&
    {
        return std::move(*static_cast<NormalizedType<T>*>(m_storage.GetPointer()));
    }

    template <class T>
    HYP_FORCE_INLINE T* TryGet()
    {
        if (!Is<T>())
        {
            return nullptr;
        }

        return static_cast<T*>(m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE const T* TryGet() const
    {
        if (!Is<T>())
        {
            return nullptr;
        }

        return static_cast<const T*>(m_storage.GetPointer());
    }

    template <class T, typename = std::enable_if_t<std::is_copy_constructible_v<T>>>
    void Set(const T& value)
    {
        static_assert(Helper::template holdsType<T>, "Type is not valid for the variant");

        if constexpr (!VariantHelper<Types...>::triviallyDestructible)
        {
            if (IsValid())
            {
                destructFunctions[m_currentIndex](CurrentTypeId(), m_storage.GetPointer());
            }
        }

        constexpr TypeId typeId = TypeId::ForType<NormalizedType<T>>();

        m_currentIndex = TypeIndexHelper<VariantBase<Types...>> {}(typeId);
        const bool constructResult = copyConstructFunctions[m_currentIndex](typeId, m_storage.GetPointer(), &value);

        HYP_CORE_ASSERT(constructResult);
    }

    template <class T>
    void Set(T&& value)
    {
        static_assert(Helper::template holdsType<T>, "Type is not valid for the variant");

        if constexpr (!VariantHelper<Types...>::triviallyDestructible)
        {
            if (IsValid())
            {
                destructFunctions[m_currentIndex](CurrentTypeId(), m_storage.GetPointer());
            }
        }

        constexpr TypeId typeId = TypeId::ForType<NormalizedType<T>>();
        m_currentIndex = TypeIndexHelper<VariantBase<Types...>> {}(typeId);

        const bool constructResult = moveConstructFunctions[m_currentIndex](typeId, m_storage.GetPointer(), &value);

        // Not a valid type for the variant
        HYP_CORE_ASSERT(constructResult);
    }

    template <class T, class... Args>
    void Emplace(Args&&... args)
    {
        static_assert(Helper::template holdsType<T>, "Type is not valid for the variant");

        if constexpr (!VariantHelper<Types...>::triviallyDestructible)
        {
            if (IsValid())
            {
                destructFunctions[m_currentIndex](CurrentTypeId(), m_storage.GetPointer());
            }
        }

        m_currentIndex = invalidTypeIndex;

        constexpr TypeId typeId = TypeId::ForType<NormalizedType<T>>();

        Memory::Construct<NormalizedType<T>>(m_storage.GetPointer(), std::forward<Args>(args)...);

        m_currentIndex = TypeIndexHelper<VariantBase<Types...>> {}(typeId);
    }

    /*! \brief Resets the Variant into an invalid state.
     * If there is any present value, it will be destructed
     */
    void Reset()
    {
        if constexpr (!VariantHelper<Types...>::triviallyDestructible)
        {
            if (IsValid())
            {
                destructFunctions[m_currentIndex](CurrentTypeId(), m_storage.GetPointer());
            }
        }

        m_currentIndex = invalidTypeIndex;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (!IsValid())
        {
            return HashCode();
        }

        return VariantHelper<Types...>::GetHashCode(CurrentTypeId(), m_storage.GetPointer());
    }

protected:
    static constexpr SizeType maxSize = MathUtil::Max(sizeof(Types)...);
    static constexpr SizeType maxAlign = MathUtil::Max(alignof(Types)...);

    int m_currentIndex;

    struct alignas(maxAlign) Storage
    {
        alignas(maxAlign) ubyte dataBuffer[maxSize];

        void* GetPointer()
        {
            return static_cast<void*>(&dataBuffer[0]);
        }

        const void* GetPointer() const
        {
            return static_cast<const void*>(&dataBuffer[0]);
        }
    } m_storage;

    HYP_FORCE_INLINE constexpr TypeId CurrentTypeId() const
    {
        return typeIds[m_currentIndex + 1];
    }
};

template <bool IsCopyable, class... Types>
struct VariantHolder : public VariantBase<Types...>
{
};

template <class... Types>
struct VariantHolder<true, Types...> : public VariantBase<Types...>
{
private:
    using Base = VariantBase<Types...>;

public:
    VariantHolder() = default;

    VariantHolder(VariantHolder&& other) noexcept
        : Base(std::move(other))
    {
    }

    VariantHolder& operator=(VariantHolder&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    VariantHolder(const VariantHolder& other)
        : Base()
    {
        if (other.IsValid())
        {
            const bool constructResult = Base::copyConstructFunctions[other.m_currentIndex](other.CurrentTypeId(), Base::m_storage.GetPointer(), other.m_storage.GetPointer());

            // Not compatible
            HYP_CORE_ASSERT(constructResult);

            Base::m_currentIndex = other.m_currentIndex;
        }
    }

    VariantHolder& operator=(const VariantHolder& other)
    {
        if (std::addressof(other) == this)
        {
            return *this;
        }

        if (Base::IsValid())
        {
            if (other.m_currentIndex == Base::m_currentIndex)
            {
                const bool assignResult = Base::copyAssignFunctions[Base::m_currentIndex](Base::CurrentTypeId(), Base::m_storage.GetPointer(), other.m_storage.GetPointer());

                HYP_CORE_ASSERT(assignResult);
            }
            else
            {
                if constexpr (!VariantHelper<Types...>::triviallyDestructible)
                {
                    Base::destructFunctions[Base::m_currentIndex](Base::CurrentTypeId(), Base::m_storage.GetPointer());
                }

                Base::m_currentIndex = Base::invalidTypeIndex;

                if (other.IsValid())
                {
                    const bool constructResult = Base::copyConstructFunctions[other.m_currentIndex](other.CurrentTypeId(), Base::m_storage.GetPointer(), other.m_storage.GetPointer());

                    HYP_CORE_ASSERT(constructResult);

                    Base::m_currentIndex = other.m_currentIndex;
                }
            }
        }
        else if (other.IsValid())
        {
            const bool constructResult = Base::copyConstructFunctions[other.m_currentIndex](other.CurrentTypeId(), Base::m_storage.GetPointer(), other.m_storage.GetPointer());

            HYP_CORE_ASSERT(constructResult);

            Base::m_currentIndex = other.m_currentIndex;
        }

        HYP_CORE_ASSERT(Base::m_currentIndex == other.m_currentIndex);

        return *this;
    }

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, VariantHolder> && std::is_copy_constructible_v<T>>>
    explicit VariantHolder(const T& value)
        : Base(value)
    {
    }

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, VariantHolder>>>
    explicit VariantHolder(T&& value) noexcept
        : Base(std::forward<T>(value))
    {
    }

    ~VariantHolder() = default;
};

template <class... Types>
struct VariantHolder<false, Types...> : public VariantBase<Types...>
{
private:
    using Base = VariantBase<Types...>;

public:
    VariantHolder() = default;

    VariantHolder(VariantHolder&& other) noexcept
        : Base(std::move(other))
    {
    }

    VariantHolder& operator=(VariantHolder&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    VariantHolder(const VariantHolder& other) = delete;
    VariantHolder& operator=(const VariantHolder& other) = delete;

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, VariantHolder> && std::is_copy_constructible_v<T>>>
    explicit VariantHolder(const T& value)
        : Base(value)
    {
    }

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, VariantHolder>>>
    explicit VariantHolder(T&& value) noexcept
        : Base(std::forward<T>(value))
    {
    }

    ~VariantHolder() = default;
};

template <class... Types>
struct Variant;

template <class VariantType, class FunctionType>
static inline void Visit(VariantType&& variant, FunctionType&& fn);

template <class... Types>
struct Variant : private ConstructAssignmentTraits<true, utilities::VariantHelper<Types...>::copyConstructible, utilities::VariantHelper<Types...>::moveConstructible, Variant<Types...>>
{
    static constexpr int invalidTypeIndex = utilities::VariantBase<Types...>::invalidTypeIndex;
    
    // we do sizeof...(Types) + 1 so getting type id from index is just accessing the element at type index + 1.
    static constexpr TypeId typeIds[sizeof...(Types) + 1] { TypeId::Void(), TypeId::ForType<Types>()... };

    // template <class ...Args>
    // Variant(Args &&... args)
    //     : m_holder(std::forward<Args>(args)...)
    // {
    // }

    Variant() = default;
    Variant(const Variant& other) = default;
    Variant& operator=(const Variant& other) = default;
    Variant(Variant&& other) noexcept = default;
    Variant& operator=(Variant&& other) noexcept = default;
    ~Variant() = default;

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant> && std::is_copy_constructible_v<T>>>
    Variant(const T& value)
        : m_holder(value)
    {
    }

    template <class T, typename = typename std::enable_if_t<!std::is_same_v<T, Variant>>>
    Variant(T&& value) noexcept
        : m_holder(std::forward<T>(value))
    {
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return m_holder.GetTypeId();
    }

    HYP_FORCE_INLINE int GetTypeIndex() const
    {
        return m_holder.GetTypeIndex();
    }

    HYP_FORCE_INLINE bool operator==(const Variant& other) const
    {
        return m_holder == other.m_holder;
    }

    HYP_FORCE_INLINE bool operator!=(const Variant& other) const
    {
        return !(m_holder == other.m_holder);
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_holder.IsValid();
    }

    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        return m_holder.template Is<T>();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_holder.IsValid();
    }

    HYP_FORCE_INLINE bool HasValue() const
    {
        return m_holder.IsValid();
    }

    HYP_FORCE_INLINE void* GetPointer() &
    {
        return m_holder.GetPointer();
    }

    HYP_FORCE_INLINE const void* GetPointer() const&
    {
        return m_holder.GetPointer();
    }

    template <class T, class ReturnType = NormalizedType<T>>
    HYP_FORCE_INLINE bool Get(ReturnType* outValue) const
    {
        return m_holder.template Get<T, ReturnType>(outValue);
    }

    template <class T>
    HYP_FORCE_INLINE T& Get() &
    {
        return m_holder.template Get<T>();
    }

    template <class T>
    HYP_FORCE_INLINE const T& Get() const&
    {
        return m_holder.template Get<T>();
    }

    template <class T>
    HYP_FORCE_INLINE T Get() &&
    {
        return std::move(m_holder).template Get<T>();
    }

    template <class T>
    HYP_FORCE_INLINE T& GetUnchecked() &
    {
        return m_holder.template GetUnchecked<T>();
    }

    template <class T>
    HYP_FORCE_INLINE const T& GetUnchecked() const&
    {
        return m_holder.template GetUnchecked<T>();
    }

    template <class T>
    HYP_FORCE_INLINE T GetUnchecked() &&
    {
        return std::move(m_holder).template GetUnchecked<T>();
    }

    template <class T>
    HYP_FORCE_INLINE T* TryGet() &
    {
        return m_holder.template TryGet<T>();
    }

    template <class T>
    HYP_FORCE_INLINE const T* TryGet() const&
    {
        return m_holder.template TryGet<T>();
    }

    template <class T>
    HYP_FORCE_INLINE T& TryGet(T& defaultValue) &
    {
        if (Is<T>())
        {
            return Get<T>();
        }

        return defaultValue;
    }

    template <class T>
    HYP_FORCE_INLINE const T& TryGet(const T& defaultValue) const&
    {
        if (Is<T>())
        {
            return Get<T>();
        }

        return defaultValue;
    }

    template <class T>
    HYP_FORCE_INLINE T TryGet(T&& defaultValue) const
    {
        if (Is<T>())
        {
            return Get<T>();
        }

        return std::forward<T>(defaultValue);
    }

    template <class T>
    HYP_FORCE_INLINE T TryGet(const T& defaultValue) &&
    {
        if (Is<T>())
        {
            return Get<T>();
        }

        return defaultValue;
    }

    template <class T, typename = std::enable_if_t<std::is_copy_constructible_v<T>>>
    HYP_FORCE_INLINE void Set(const T& value)
    {
        m_holder.template Set<T>(value);
    }

    template <class T>
    HYP_FORCE_INLINE void Set(T&& value)
    {
        return m_holder.template Set<T>(std::forward<T>(value));
    }

    template <class T, class... Args>
    HYP_FORCE_INLINE void Emplace(Args&&... args)
    {
        return m_holder.template Emplace<T>(std::forward<Args>(args)...);
    }

    /*! \brief Resets the Variant into an invalid state.
     * If there is any present value, it will be destructed
     */
    HYP_FORCE_INLINE void Reset()
    {
        m_holder.Reset();
    }

    HYP_FORCE_INLINE AnyRef ToRef()
    {
        if (!IsValid())
        {
            return AnyRef();
        }

        return AnyRef(m_holder.GetTypeId(), m_holder.GetPointer());
    }

    HYP_FORCE_INLINE ConstAnyRef ToRef() const
    {
        if (!IsValid())
        {
            return ConstAnyRef();
        }

        return ConstAnyRef(m_holder.GetTypeId(), m_holder.GetPointer());
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return m_holder.GetHashCode();
    }

    template <class FunctionType>
    HYP_FORCE_INLINE void Visit(FunctionType&& fn) &
    {
        ::hyperion::utilities::Visit(*this, std::forward<FunctionType>(fn));
    }

    template <class FunctionType>
    HYP_FORCE_INLINE void Visit(FunctionType&& fn) const&
    {
        ::hyperion::utilities::Visit(*this, std::forward<FunctionType>(fn));
    }

    template <class FunctionType>
    HYP_FORCE_INLINE void Visit(FunctionType&& fn) &&
    {
        ::hyperion::utilities::Visit(std::move(*this), std::forward<FunctionType>(fn));
    }

private:
    utilities::VariantHolder<
        utilities::VariantHelper<Types...>::copyConstructible,
        Types...>
        m_holder;
};

#pragma region TypeIndex

template <class T>
struct TypeIndex_Impl
{
    constexpr bool operator()(TypeId typeId, int& index) const
    {
        constexpr TypeId otherTypeId = TypeId::ForType<T>();

        if (typeId != otherTypeId)
        {
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
    constexpr int operator()(TypeId typeId) const
    {
        int index = 0;

        if (!TypeIndex_Impl<T> {}(typeId, index))
        {
            return -1;
        }

        return index;
    }
};

template <class T, class... Types>
struct TypeIndexHelper<VariantBase<T, Types...>>
{
    constexpr int operator()(TypeId typeId) const
    {
        int value = 0;

        if (!(TypeIndex_Impl<T> {}(typeId, value) || (TypeIndex_Impl<Types> {}(typeId, value) || ...)))
        {
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
static inline void Variant_InvokeFunction(VariantType& variant, FunctionType& fn)
{
    fn(variant.template GetUnchecked<T>());
}

template <class VariantType, class FunctionType, class T>
static inline void Variant_InvokeFunction(const VariantType& variant, FunctionType& fn)
{
    fn(variant.template GetUnchecked<T>());
}

template <class VariantType, class FunctionType, class T>
static inline void Variant_InvokeFunction(VariantType&& variant, FunctionType& fn)
{
    fn(std::move(variant).template GetUnchecked<T>());
}

template <class VariantType>
struct VisitHelper;

template <class... Types>
struct VisitHelper<utilities::Variant<Types...>>
{
    template <class FunctionType>
    static inline void Invoke(utilities::Variant<Types...>& variant, FunctionType&& fn)
    {
        using InvokeFunctionWrapper = std::add_pointer_t<void(utilities::Variant<Types...>&, FunctionType&)>;

        constexpr InvokeFunctionWrapper invokeFns[sizeof...(Types)] = {
            &Variant_InvokeFunction<utilities::Variant<Types...>, FunctionType, Types>...
        };

        if (!variant.IsValid())
        {
            return;
        }

        const int typeIndex = variant.GetTypeIndex();

        // Sanity check
        HYP_CORE_ASSERT(typeIndex < sizeof...(Types));

        invokeFns[typeIndex](variant, fn);
    }

    template <class FunctionType>
    static inline void Invoke(const utilities::Variant<Types...>& variant, FunctionType&& fn)
    {
        using InvokeFunctionWrapper = std::add_pointer_t<void(const utilities::Variant<Types...>&, FunctionType&)>;

        constexpr InvokeFunctionWrapper invokeFns[sizeof...(Types)] = {
            &Variant_InvokeFunction<utilities::Variant<Types...>, FunctionType, Types>...
        };

        if (!variant.IsValid())
        {
            return;
        }

        const int typeIndex = variant.GetTypeIndex();

        // Sanity check
        HYP_CORE_ASSERT(typeIndex < sizeof...(Types));

        invokeFns[typeIndex](variant, fn);
    }

    template <class FunctionType>
    static inline void Invoke(utilities::Variant<Types...>&& variant, FunctionType&& fn)
    {
        using InvokeFunctionWrapper = std::add_pointer_t<void(utilities::Variant<Types...>&&, FunctionType&)>;

        constexpr InvokeFunctionWrapper invokeFns[sizeof...(Types)] = {
            &Variant_InvokeFunction<utilities::Variant<Types...>, FunctionType, Types>...
        };

        if (!variant.IsValid())
        {
            return;
        }

        const int typeIndex = variant.GetTypeIndex();

        // Sanity check
        HYP_CORE_ASSERT(typeIndex < sizeof...(Types));

        invokeFns[typeIndex](std::move(variant), fn);
    }
};

#pragma endregion VisitHelper

template <class VariantType, class FunctionType>
static inline void Visit(VariantType&& variant, FunctionType&& fn)
{
    VisitHelper<NormalizedType<VariantType>>::Invoke(std::forward<VariantType>(variant), std::forward<FunctionType>(fn));
}

} // namespace utilities

template <class... Types>
using Variant = utilities::Variant<Types...>;

using utilities::Visit;

} // namespace hyperion
