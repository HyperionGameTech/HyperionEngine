/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/TypeInfoFwd.hpp>
#include <core/reflection/TypeId.hpp>
#include <core/utilities/ByteUtil.hpp>

#include <core/memory/AnyRef.hpp>
#include <core/memory/Memory.hpp>

#include <core/debug/Debug.hpp>

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <type_traits>
#include <new>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass* GetClass(TypeId typeId);
extern HYP_API bool IsA(const HypClass* hypClass, const void* ptr, TypeId typeId);

namespace memory {

template <class T>
class UniquePtr;

class AnyBase
{
protected:
    // protected constructor to prevent instantiation of AnyBase directly
    AnyBase() = default;
};

/*! \brief A type-erased container for any type. T must be copyable/movable. */
class Any final : public AnyBase
{
    using CopyConstructor = std::add_pointer_t<void*(const void*)>;
    using DeleteFunction = std::add_pointer_t<void(void*)>;

    struct Block
    {
        const TypeInfo* typeInfo;
        void* objectPtr;
        CopyConstructor copyCtor;  // nullptr if not copyable (eg external without known T)
        DeleteFunction objectDtor; // may be nullptr for inline-owned objects
        DeleteFunction dtor;       // deletes the whole block
    };

    template <class T>
    static void* BlockCopyConstruct(const void* block)
    {
        const Block* src = static_cast<const Block*>(block);
        const T& val = *static_cast<const T*>(src->objectPtr);

        constexpr SizeType align = (alignof(Block) > alignof(T) ? alignof(Block) : alignof(T));
        constexpr SizeType headerSize = sizeof(Block);
        constexpr SizeType objAlign = alignof(T);
        const SizeType objOffset = ByteUtil::AlignAs(headerSize, objAlign);
        const SizeType totalSize = objOffset + sizeof(T);

        void* raw = ::operator new(totalSize, std::align_val_t(align));
        char* base = static_cast<char*>(raw);
        Block* hdr = new (base) Block { &TypeOf<T>(), nullptr, &Any::BlockCopyConstruct<T>, nullptr, &Any::BlockDeleter<T> };
        T* obj = ::new (base + objOffset) T(val);
        hdr->objectPtr = obj;
        return hdr;
    }

    template <class T>
    static void BlockDeleter(void* block)
    {
        constexpr SizeType kAlign = (alignof(Block) > alignof(T) ? alignof(Block) : alignof(T));
        Block* hdr = static_cast<Block*>(block);
        static_cast<T*>(hdr->objectPtr)->~T();
        ::operator delete(block, std::align_val_t(kAlign));
    }

    static void ExternalBlockDeleter(void* block)
    {
        Block* hdr = static_cast<Block*>(block);
        if (hdr->objectPtr && hdr->objectDtor)
        {
            hdr->objectDtor(hdr->objectPtr);
        }
        ::operator delete(block, std::align_val_t(alignof(Block)));
    }

public:
    // UniquePtr is a friend class
    template <class T>
    friend class memory::UniquePtr;

    Any()
        : m_block(nullptr)
    {
    }

    /*! \brief Construct a new T into the Any, without needing to use any move or copy constructors. */
    template <class T, class... Args>
    static Any Construct(Args&&... args)
    {
        Any any;

        if constexpr (!std::is_void_v<T>)
        {
            any.Emplace<NormalizedType<T>>(std::forward<Args>(args)...);
        }

        return any;
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<AnyBase, T>>>
    Any(const T& value)
        : m_block(nullptr)
    {
        using U = NormalizedType<T>;
        Emplace<U>(value);
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<AnyBase, T>>>
    Any& operator=(const T& value)
    {
        using U = NormalizedType<T>;
        const TypeId newTypeId = TypeId::ForType<U>();

        if constexpr (std::is_copy_assignable_v<U>)
        {
            if (GetTypeId() == newTypeId)
            {
                *static_cast<U*>(GetPointer()) = value;
                return *this;
            }
        }

        if (HasValue())
        {
            reinterpret_cast<Block*>(m_block)->dtor(m_block);
        }

        Emplace<U>(value);
        return *this;
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<AnyBase, T>>>
    Any(T&& value) noexcept
        : m_block(nullptr)
    {
        using U = NormalizedType<T>;
        Emplace<U>(std::forward<T>(value));
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<AnyBase, T>>>
    Any& operator=(T&& value) noexcept
    {
        using U = NormalizedType<T>;
        const TypeId newTypeId = TypeId::ForType<U>();

        if constexpr (std::is_move_assignable_v<U>)
        {
            if (GetTypeId() == newTypeId)
            {
                *static_cast<U*>(GetPointer()) = std::move(value);
                return *this;
            }
        }

        if (HasValue())
        {
            reinterpret_cast<Block*>(m_block)->dtor(m_block);
        }

        Emplace<U>(std::move(value));
        return *this;
    }

    Any(const Any& other)
        : m_block(nullptr)
    {
        if (other.HasValue())
        {
            const Block* ob = reinterpret_cast<const Block*>(other.m_block);
            m_block = ob->copyCtor ? ob->copyCtor(other.m_block) : nullptr;
        }
    }

    Any& operator=(const Any& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if (HasValue())
        {
            reinterpret_cast<Block*>(m_block)->dtor(m_block);
        }

        if (other.HasValue())
        {
            const Block* ob = reinterpret_cast<const Block*>(other.m_block);
            m_block = ob->copyCtor ? ob->copyCtor(other.m_block) : nullptr;
        }
        else
        {
            m_block = nullptr;
        }

        return *this;
    }

    Any(Any&& other) noexcept
        : m_block(other.m_block)
    {
        other.m_block = nullptr;
    }

    Any& operator=(Any&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (HasValue())
        {
            reinterpret_cast<Block*>(m_block)->dtor(m_block);
        }

        m_block = other.m_block;
        other.m_block = nullptr;

        return *this;
    }

    ~Any()
    {
        if (HasValue())
        {
            reinterpret_cast<Block*>(m_block)->dtor(m_block);
        }
    }

    HYP_FORCE_INLINE bool operator==(const Any& other) const = delete;
    HYP_FORCE_INLINE bool operator!=(const Any& other) const = delete;

    /*! \brief Get a raw pointer to the held object. */
    HYP_FORCE_INLINE void* GetPointer()
    {
        return HasValue() ? reinterpret_cast<Block*>(m_block)->objectPtr : nullptr;
    }

    /*! \brief Get a raw pointer to the held object. */
    HYP_FORCE_INLINE const void* GetPointer() const
    {
        return HasValue() ? reinterpret_cast<const Block*>(m_block)->objectPtr : nullptr;
    }

    /*! \brief Returns true if the Any has a value. */
    HYP_FORCE_INLINE bool HasValue() const
    {
        return m_block != nullptr;
    }

    /*! \brief Returns the TypeId of the held object. */
    TypeId GetTypeId() const;

    HYP_FORCE_INLINE const TypeInfo* GetTypeInfo() const
    {
        return HasValue() ? reinterpret_cast<const Block*>(m_block)->typeInfo : &TypeInfo_Void();
    }

    /*! \brief Returns true if the held object is of type T.
     *  If T has a HypClass registered, this function will also return true if the held object is a subclass of T. */
    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        const TypeId typeId = TypeId::ForType<NormalizedType<T>>();
        const void* ptr = GetPointer();
        const TypeId held = GetTypeId();
        return held == typeId || IsA(GetClass(typeId), ptr, held);
    }

    /*! \brief Returns true if the held object is of type \ref{typeId}.
     *  If the type with the given Id has a HypClass registered, this function will also return true if the held object is a subclass of the type. */
    HYP_FORCE_INLINE bool Is(TypeId typeId) const
    {
        const void* ptr = GetPointer();
        const TypeId held = GetTypeId();
        return held == typeId || IsA(GetClass(typeId), ptr, held);
    }

    /*! \brief Returns the held object as a reference to type T. If the held object is not of type T, an assertion will fail. */
    template <class T>
    HYP_FORCE_INLINE T& Get() const
    {
        const TypeId requestedTypeId = TypeId::ForType<NormalizedType<T>>();
        HYP_CORE_ASSERT(GetTypeId() == requestedTypeId, "Held type not equal to requested type!");

        return *static_cast<NormalizedType<T>*>(reinterpret_cast<Block*>(m_block)->objectPtr);
    }

    /*! \brief Attempts to get the held object as a pointer to type T. If the held object is not of type T, nullptr is returned. */
    template <class T>
    HYP_FORCE_INLINE T* TryGet() const
    {
        const TypeId requestedTypeId = TypeId::ForType<NormalizedType<T>>();

        if (GetTypeId() == requestedTypeId)
        {
            return static_cast<NormalizedType<T>*>(reinterpret_cast<Block*>(m_block)->objectPtr);
        }

        return nullptr;
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<AnyBase, T>>>
    void Set(const T& value)
    {
        using U = NormalizedType<T>;
        if (HasValue())
        {
            reinterpret_cast<Block*>(m_block)->dtor(m_block);
        }

        Emplace<U>(value);
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<AnyBase, T>>>
    void Set(T&& value)
    {
        using U = NormalizedType<T>;
        if (HasValue())
        {
            reinterpret_cast<Block*>(m_block)->dtor(m_block);
        }

        Emplace<U>(std::move(value));
    }

    /*! \brief Construct a new pointer into the Any. Any current value will be destroyed. */
    template <class T, class... Args>
    T& Emplace(Args&&... args)
    {
        using U = NormalizedType<T>;
        if (HasValue())
        {
            reinterpret_cast<Block*>(m_block)->dtor(m_block);
        }

        constexpr SizeType align = (alignof(Block) > alignof(U) ? alignof(Block) : alignof(U));
        constexpr SizeType headerSize = sizeof(Block);
        constexpr SizeType objAlign = alignof(U);
        const SizeType objOffset = ByteUtil::AlignAs(headerSize, objAlign);
        const SizeType totalSize = objOffset + sizeof(U);

        void* raw = ::operator new(totalSize, std::align_val_t(align));
        char* base = static_cast<char*>(raw);
        Block* hdr = new (base) Block { &TypeOf<U>(), nullptr, &Any::BlockCopyConstruct<U>, nullptr, &Any::BlockDeleter<U> };
        U* obj = ::new (base + objOffset) U(std::forward<Args>(args)...);
        hdr->objectPtr = obj;

        m_block = hdr;
        return *obj;
    }

    /*! \brief Takes ownership of {ptr}, resetting the current value held in the Any.
        Do NOT delete the value passed to this function, as it is deleted by the Any.
    */
    template <class T>
    HYP_FORCE_INLINE void Reset(T* ptr)
    {
        using U = NormalizedType<T>;
        if (HasValue())
        {
            reinterpret_cast<Block*>(m_block)->dtor(m_block);
        }

        if (ptr)
        {
            void* raw = ::operator new(sizeof(Block), std::align_val_t(alignof(Block)));
            Block* hdr = new (raw) Block { &TypeOf<U>(), ptr, &Any::BlockCopyConstruct<U>, &Memory::Delete<U>, &Any::ExternalBlockDeleter };
            m_block = hdr;
        }
        else
        {
            m_block = nullptr;
        }
    }

    static Any FromVoidPointer(const TypeInfo* typeInfo, void* ptr, CopyConstructor copyCtor, DeleteFunction dtor)
    {
        HYP_CORE_ASSERT(typeInfo != nullptr, "typeInfo must not be null");

        Any result;
        if (ptr)
        {
            void* raw = ::operator new(sizeof(Block), std::align_val_t(alignof(Block)));
            Block* hdr = new (raw) Block { typeInfo, ptr, copyCtor, dtor, &Any::ExternalBlockDeleter };
            result.m_block = hdr;
        }
        return result;
    }

    /*! \brief Resets the current value held in the Any. */
    HYP_FORCE_INLINE void Reset()
    {
        if (HasValue())
        {
            reinterpret_cast<Block*>(m_block)->dtor(m_block);
        }

        m_block = nullptr;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return HasValue();
    }

    /*! \brief Returns the held object as a reference to type T */
    HYP_NODISCARD HYP_FORCE_INLINE AnyRef ToRef()
    {
        return AnyRef(GetTypeInfo(), GetPointer());
    }

    /*! \brief Returns the held object as a const reference to type T */
    HYP_NODISCARD HYP_FORCE_INLINE ConstAnyRef ToRef() const
    {
        return ConstAnyRef(GetTypeInfo(), GetPointer());
    }

    HYP_NODISCARD HYP_FORCE_INLINE explicit operator AnyRef()
    {
        return AnyRef(GetTypeInfo(), GetPointer());
    }

    HYP_NODISCARD HYP_FORCE_INLINE explicit operator ConstAnyRef() const
    {
        return ConstAnyRef(GetTypeInfo(), GetPointer());
    }

protected:
    void* m_block;
};

} // namespace memory

using memory::Any;
using memory::AnyBase;

} // namespace hyperion