/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/TypeInfoFwd.hpp>
#include <Core/Reflection/TypeId.hpp>
#include <Core/Utilities/ByteUtil.hpp>

#include <Core/Memory/AnyRef.hpp>
#include <Core/Memory/Memory.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <type_traits>
#include <new>

namespace Hyperion {

class Class;

CORE_API extern const Class* GetClass(const TypeId& typeId);
CORE_API extern bool IsA(const Class* cls, const void* ptr, const TypeId& typeId);

namespace memory {

class AnyBase
{
protected:
    // protected constructor to prevent instantiation of AnyBase directly
    AnyBase() = default;
};

/*! \brief A type-erased container for any type. T must be copyable/movable. */
class Any final : public AnyBase
{
public:
    using CopyConstructor = void*(*)(void* ctx, const void* src);
    using DeleteFunction = void(*)(void* ctx, void* ptr);

    struct Block
    {
        const TypeInfo* typeInfo;
        void* objectPtr;
        void* ctx;
        CopyConstructor copyCtor;  // nullptr if not copyable (eg external without known T)
        DeleteFunction objectDtor; // may be nullptr for inline-owned objects
        DeleteFunction dtor;       // deletes the whole block
        size_t objSize = 0;
        size_t objAlign = 0;
    };

    template <class T, class AllocatorType = DynamicAllocator>
    static void* BlockCopyConstruct(void* /* ctx */, const void* block)
    {
        static AllocatorType* s_allocator = GetDefaultAllocatorInstance<AllocatorType>();

        const Block* src = static_cast<const Block*>(block);
        const T& val = *static_cast<const T*>(src->objectPtr);

        constexpr size_t align = (alignof(Block) > alignof(T) ? alignof(Block) : alignof(T));
        constexpr size_t headerSize = sizeof(Block);
        constexpr size_t objAlign = alignof(T);
        constexpr size_t objOffset = ByteUtil::AlignAs(headerSize, objAlign);
        constexpr size_t totalSize = ByteUtil::AlignAs(objOffset + sizeof(T), align);

        void* raw = s_allocator->Allocate(totalSize, align);
        HYP_CORE_ASSERT(raw != nullptr);

        char* base = static_cast<char*>(raw);

        Block* hdr = new (base) Block {
            &TypeOf<T>(),
            nullptr,
            nullptr,
            &Any::BlockCopyConstruct<T, AllocatorType>,
            nullptr,
            &Any::BlockDeleter<T, AllocatorType>,
            sizeof(T),
            alignof(T)
        };

        T* obj = ::new (base + objOffset) T(val);
        hdr->objectPtr = obj;

        return hdr;
    }

    template <class AllocatorType = DynamicAllocator>
    static void* BlockCopyConstruct(void* ctx, const void* block)
    {
        static AllocatorType* s_allocator = GetDefaultAllocatorInstance<AllocatorType>();

        const Block* src = static_cast<const Block*>(block);

        const size_t totalAlign = (alignof(Block) > src->objAlign ? alignof(Block) : src->objAlign);
        const size_t objOffset = ByteUtil::AlignAs(sizeof(Block), src->objAlign);
        const size_t totalSize = ByteUtil::AlignAs(objOffset + src->objSize, totalAlign);

        void* raw = s_allocator->Allocate(totalSize, totalAlign);
        HYP_CORE_ASSERT(raw != nullptr);

        char* base = static_cast<char*>(raw);
        void* obj = base + objOffset;

        Memory::Copy(obj, src->objectPtr, src->objSize);

        Block* hdr = new (base) Block {
            src->typeInfo,
            nullptr,
            src->ctx,
            &Any::BlockCopyConstruct<AllocatorType>,
            src->objectDtor,
            src->dtor,
            src->objSize,
            src->objAlign
        };
        hdr->objectPtr = obj;

        return hdr;
    }

    template <class T, class AllocatorType = DynamicAllocator>
    static void BlockDeleter(void* /* ctx */, void* block)
    {
        static AllocatorType* s_allocator = GetDefaultAllocatorInstance<AllocatorType>();

        Block* hdr = static_cast<Block*>(block);
        static_cast<T*>(hdr->objectPtr)->~T();

        s_allocator->Free(block);
    }

    template <class AllocatorType = DynamicAllocator>
    static void ExternalBlockDeleter(void* /* ctx */, void* block)
    {
        static AllocatorType* s_allocator = GetDefaultAllocatorInstance<AllocatorType>();

        Block* hdr = static_cast<Block*>(block);
        if (hdr->objectPtr && hdr->objectDtor)
        {
            hdr->objectDtor(hdr->ctx, hdr->objectPtr);
        }

        s_allocator->Free(block);
    }

public:
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
            Block* block = reinterpret_cast<Block*>(m_block);
            block->dtor(block->ctx, m_block);
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
            Block* block = reinterpret_cast<Block*>(m_block);
            block->dtor(block->ctx, m_block);
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
            m_block = ob->copyCtor ? ob->copyCtor(ob->ctx, other.m_block) : nullptr;
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
            Block* block = reinterpret_cast<Block*>(m_block);
            block->dtor(block->ctx, m_block);
        }

        if (other.HasValue())
        {
            const Block* ob = reinterpret_cast<const Block*>(other.m_block);
            m_block = ob->copyCtor ? ob->copyCtor(ob->ctx, other.m_block) : nullptr;
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
            Block* block = reinterpret_cast<Block*>(m_block);
            block->dtor(block->ctx, m_block);
        }

        m_block = other.m_block;
        other.m_block = nullptr;

        return *this;
    }

    ~Any()
    {
        if (HasValue())
        {
            Block* block = reinterpret_cast<Block*>(m_block);
            block->dtor(block->ctx, m_block);
        }
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return HasValue();
    }

    HYP_FORCE_INLINE bool operator !() const
    {
        return !HasValue();
    }

    /*! \brief Returns true if the Any has a value. */
    HYP_FORCE_INLINE bool HasValue() const
    {
        return m_block != nullptr;
    }

    /*! \important Equals comparison for Any type just compares references, does not check if values are equal */
    HYP_FORCE_INLINE bool operator==(const Any& other) const
    {
        return GetPointer() == other.GetPointer();
    }

    /*! \important Equals comparison for Any type just compares references, does not check if values are equal */
    HYP_FORCE_INLINE bool operator!=(const Any& other) const
    {
        return GetPointer() != other.GetPointer();
    }

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

    /*! \returns The TypeInfo for the internally held object. */
    HYP_FORCE_INLINE const TypeInfo* GetTypeInfo() const
    {
        return HasValue() ? reinterpret_cast<const Block*>(m_block)->typeInfo : &TypeInfo_Void();
    }

    /*! \returns The TypeId of the held object; If no object is held, TypeId::Void() will be returned. */
    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return TypeInfo_GetId(*GetTypeInfo());
    }

    /*! \brief Returns true if the held object is of type T.
     *  If T has a Class registered, this function will also return true if the held object is a subclass of T. */
    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        const TypeId typeId = TypeId::ForType<NormalizedType<T>>();
        const void* ptr = GetPointer();
        const TypeId held = GetTypeId();
        return held == typeId || IsA(Hyperion::GetClass(typeId), ptr, held);
    }

    /*! \brief Returns true if the held object is of type \p typeId.
     *  If the type with the given Id has a Class registered, this function will also return true if the held object is a subclass of the type. */
    HYP_FORCE_INLINE bool Is(TypeId typeId) const
    {
        const void* ptr = GetPointer();
        const TypeId held = GetTypeId();
        return held == typeId || IsA(Hyperion::GetClass(typeId), ptr, held);
    }

    /*! \brief Returns the held object as a reference to type T. If the held object is not of type T, an assertion will fail. */
    template <class T>
    HYP_FORCE_INLINE T& Get() const
    {
        HYP_CORE_ASSERT(Is<T>(), "Held type not equal to requested type!");

        return *static_cast<NormalizedType<T>*>(reinterpret_cast<Block*>(m_block)->objectPtr);
    }

    /*! \brief Attempts to get the held object as a pointer to type T. If the held object is not of type T, nullptr is returned. */
    template <class T>
    HYP_FORCE_INLINE T* TryGet() const
    {
        if (Is<T>())
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
            Block* block = reinterpret_cast<Block*>(m_block);
            block->dtor(block->ctx, m_block);
        }

        Emplace<U>(value);
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<AnyBase, T>>>
    void Set(T&& value)
    {
        using U = NormalizedType<T>;
        if (HasValue())
        {
            Block* block = reinterpret_cast<Block*>(m_block);
            block->dtor(block->ctx, m_block);
        }

        Emplace<U>(std::move(value));
    }

    /*! \brief Construct a new pointer into the Any. Any current value will be destroyed. */
    template <class T, class... Args>
    T& Emplace(Args&&... args)
    {
        using U = NormalizedType<T>;

        using AllocatorType = DynamicAllocator;
        static AllocatorType* s_allocator = GetDefaultAllocatorInstance<AllocatorType>();


        if (HasValue())
        {
            Block* block = reinterpret_cast<Block*>(m_block);
            block->dtor(block->ctx, m_block);
        }

        constexpr size_t align = (alignof(Block) > alignof(U) ? alignof(Block) : alignof(U));
        constexpr size_t headerSize = sizeof(Block);
        constexpr size_t objAlign = alignof(U);
        constexpr size_t objOffset = ByteUtil::AlignAs(headerSize, objAlign);
        constexpr size_t totalSize = ByteUtil::AlignAs(objOffset + sizeof(U), align);

        void* raw = s_allocator->Allocate(totalSize, align);
        HYP_CORE_ASSERT(raw != nullptr);

        char* base = static_cast<char*>(raw);

        Block* hdr = new (base) Block {
            &TypeOf<U>(),
            nullptr,
            nullptr,
            &Any::BlockCopyConstruct<U, AllocatorType>,
            nullptr,
            &Any::BlockDeleter<U, AllocatorType>,
            sizeof(U),
            alignof(U)
        };

        U* obj = ::new (base + objOffset) U(std::forward<Args>(args)...);
        hdr->objectPtr = obj;

        m_block = hdr;

        return *obj;
    }

    /*! \brief Takes ownership of {ptr}, resetting the current value held in the Any.
        Do NOT delete the value passed to this function, as it is deleted by the Any.
    */
    template <class T, class AllocatorType = DynamicAllocator>
    HYP_FORCE_INLINE void Reset(T* ptr)
    {
        using U = NormalizedType<T>;

        if (HasValue())
        {
            Block* block = reinterpret_cast<Block*>(m_block);
            block->dtor(block->ctx, m_block);
        }

        if (ptr)
        {
            static AllocatorType* s_allocator = GetDefaultAllocatorInstance<AllocatorType>();

            void* raw = s_allocator->Allocate(sizeof(Block), alignof(Block));
            HYP_CORE_ASSERT(raw != nullptr);

            Block* hdr = new (raw) Block {
                &TypeOf<U>(),
                ptr,
                nullptr,
                &Any::BlockCopyConstruct<U, AllocatorType>,
                [](void* ctx, void* ptr)
                {
                    static_cast<U*>(ptr)->~U();
                    s_allocator->Free(ptr);
                },
                &Any::ExternalBlockDeleter<AllocatorType>,
                sizeof(U),
                alignof(U)
            };

            m_block = hdr;
        }
        else
        {
            m_block = nullptr;
        }
    }

    template <class AllocatorType>
    static Any FromVoidPointer(const TypeInfo* typeInfo, void* ptr, CopyConstructor copyCtor, DeleteFunction dtor, void* ctx = nullptr, size_t objSize = 0, size_t objAlign = 0)
    {
        HYP_CORE_ASSERT(typeInfo != nullptr, "typeInfo must not be null");

        Any result;

        if (ptr)
        {
            static AllocatorType* s_allocator = GetDefaultAllocatorInstance<AllocatorType>();

            void* raw = s_allocator->Allocate(sizeof(Block), alignof(Block));
            HYP_CORE_ASSERT(raw != nullptr);

            Block* hdr = new (raw) Block {
                typeInfo,
                ptr,
                ctx,
                copyCtor,
                dtor,
                &Any::ExternalBlockDeleter<AllocatorType>,
                objSize,
                objAlign
            };

            result.m_block = hdr;
        }

        return result;
    }

    /*! \brief Resets the current value held in the Any. */
    HYP_FORCE_INLINE void Reset()
    {
        if (HasValue())
        {
            Block* block = reinterpret_cast<Block*>(m_block);
            block->dtor(block->ctx, m_block);
        }

        m_block = nullptr;
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

} // namespace Hyperion
