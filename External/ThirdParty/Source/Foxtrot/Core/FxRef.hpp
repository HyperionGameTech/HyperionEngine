#pragma once

#include "FxDefines.hpp"
#include "FxPanic.hpp"
#include "FxTypes.hpp"

#ifdef FX_DEBUG_REF
#include <Core/FxLog.hpp>
#endif

#include <Core/MemPool/FxMemPool.hpp>
#include <FxEngine.hpp>
#include <Math/FxMathUtil.hpp>
#include <cstddef>

/** The internal reference count for `FxRef`. */
struct FxRefCount
{
    using IntType = uint32;
    using RefCountType = IntType;

    /** Increments the reference count */
    void Inc() { ++Count; }

    /** Decrements the reference count */
    IntType Dec() { return (--Count); }

public:
    RefCountType Count = 1;
};

template <typename T>
class FxRef
{
public:
    using UserType = T;

    FxRef() : mpRefCnt(nullptr), mpPtr(nullptr) {}
    FxRef(nullptr_t np) : mpRefCnt(nullptr), mpPtr(nullptr) {}

    /**
     * Constructs a new FxRef from a pointer.
     */
    FxRef(T* ptr) : FxRef(ptr, gEnginePool->Alloc<FxRefCount>(sizeof(FxRefCount))) {}

    /**
     * Constructs a new FxRef from a pointer and a pre-allocated ref count.
     */
    FxRef(T* ptr, FxRefCount* cnt, bool is_combined_allocation = false)
        : mpRefCnt(cnt), mpPtr(ptr), mbIsCombinedAllocation(is_combined_allocation), mbIsExternalPtr(true)
    {
#ifdef FX_DEBUG_REF
        FxLogDebug("Created FxRef {:p} (RefCnt: {:p})", reinterpret_cast<void*>(mpPtr),
                   reinterpret_cast<void*>(mpRefCnt));
#endif
    }

    /**
     * Inherits from another FxRef that has a type that is convertible to T.
     *
     * @tparam U The type of the other FxRef
     */
    template <typename DerivedType>
        requires std::is_base_of_v<T, DerivedType>
    FxRef(const FxRef<DerivedType>& other)
    {
        mpRefCnt = other.mpRefCnt;
        mpPtr = other.mpPtr;
        mbIsCombinedAllocation = other.mbIsCombinedAllocation;
        mbIsExternalPtr = other.mbIsExternalPtr;

        if (mpRefCnt) {
            mpRefCnt->Inc();
        }
    }

    /**
     * @brief Converts from a base type FxRef to a derived type FxRef.
     */
    template <typename BaseType>
        requires std::is_base_of_v<BaseType, T>
    FxRef(const FxRef<BaseType>& other)
    {
        mpRefCnt = other.mpRefCnt;
        mpPtr = static_cast<T*>(other.mpPtr);
        mbIsCombinedAllocation = other.mbIsCombinedAllocation;
        mbIsExternalPtr = other.mbIsExternalPtr;

        if (mpRefCnt) {
            mpRefCnt->Inc();
        }
    }


    /**
     * Inherits from another FxRef that contains the same type.
     */
    FxRef(const FxRef& other)
    {
        mpRefCnt = other.mpRefCnt;
        mpPtr = other.mpPtr;
        mbIsCombinedAllocation = other.mbIsCombinedAllocation;
        mbIsExternalPtr = other.mbIsExternalPtr;

        if (mpRefCnt) {
            mpRefCnt->Inc();
        }
    }


    FxRef(FxRef&& other)
    {
        mpRefCnt = (other.mpRefCnt);
        mpPtr = (other.mpPtr);
        mbIsCombinedAllocation = other.mbIsCombinedAllocation;
        mbIsExternalPtr = other.mbIsExternalPtr;

        other.mpPtr = nullptr;
        other.mpRefCnt = nullptr;
    }


    ~FxRef() { DecRef(); }

    /**
     * Constructs a new FxRef for the given type using the provided arguments.
     *
     * @param args The arguments to be forwarded to the constructor of T.
     */
    template <typename... Args>
    static FxRef<T> New(Args... args)
    {
        // Since we want to ensure aligned laod/store on the ref count object as well, we should ensure that the T
        // object is aligned on a 16 byte boundary.
        uint8* raw_ptr = gEnginePool->Alloc<uint8>(FxMath::AlignValue<16>(sizeof(T)) + sizeof(FxRefCount));

        T* obj_ptr = reinterpret_cast<T*>(raw_ptr);
        FxRefCount* count_ptr = reinterpret_cast<FxRefCount*>(raw_ptr + sizeof(T));

        FxAssert(reinterpret_cast<uint8*>(count_ptr) - sizeof(T) == raw_ptr);

        // Construct the ref count
        ::new (count_ptr) FxRefCount();

        // Construct the Ref object
        if constexpr (std::is_constructible_v<T, Args...>) {
            ::new (obj_ptr) T(std::forward<Args>(args)...);
        }

#ifdef FX_DEBUG_REF
        FxLogDebug("Creating FxRef {:p} (RefCnt: {:p}) as combined allocation", reinterpret_cast<void*>(obj_ptr),
                   reinterpret_cast<void*>(count_ptr));
#endif

        return FxRef<T>(obj_ptr, count_ptr, true);
    }

    /** Retrieves the internal usage count for the reference. */
    uint32 GetRefCount()
    {
        if (mpRefCnt) {
            return mpRefCnt->Count;
        }

        return 0;
    }

    ///////////////////////////////
    // Operator overloads
    ///////////////////////////////

    FxRef& operator=(const FxRef& other)
    {
        // If there is already a pointer in this reference, decrement or destroy it
        // This will be an infinite memory printing machine if this is not here
        if (mpPtr && mpRefCnt) {
            DecRef();
        }

        mpPtr = other.mpPtr;
        mpRefCnt = other.mpRefCnt;
        mbIsCombinedAllocation = other.mbIsCombinedAllocation;
        mbIsExternalPtr = other.mbIsExternalPtr;

        // Since `mRefCnt` now points to the old ref's count, Inc'ing this
        // marks that the object is in use.
        IncRef();

        return *this;
    }

    T* operator->() const noexcept
    {
        FxAssert(mpRefCnt != nullptr && mpPtr != nullptr);

        return mpPtr;
    }

    T& operator*() const noexcept
    {
        FxAssert((mpRefCnt != nullptr && mpPtr != nullptr));

        return *mpPtr;
    }

    bool operator==(nullptr_t np) const noexcept { return mpPtr == nullptr; }

    operator bool() const noexcept { return mpPtr != nullptr; }

    bool IsValid() const noexcept { return mpPtr != nullptr && mpRefCnt != nullptr; }

    void SetNull()
    {
        mpPtr = nullptr;
        mpRefCnt = nullptr;
    }

    void DestroyRef()
    {
        if (mpPtr) {
            // The pointer if the ref count and ptr are allocated together.
            // +=========+==========+======
            // | Obj Ptr | RefCnt   | . . .
            // +=========+==========+======
            // NOTE: Unless we want to store the size of the original object, using the casting constructor messes up
            // using mpPtr + sizeof(T) == mpRefCnt to check if the allocation is combined.

            if (mbIsCombinedAllocation) {
                // Call the destructor manually (we remove the type info so the MemPool::Free call won't call it)
                if constexpr (std::is_destructible_v<T>) {
                    mpPtr->~T();
                }

                if (mpRefCnt) {
                    // Call the destructor on the ref count
                    mpRefCnt->~FxRefCount();
                }

                // Free the bundled memory
                gEnginePool->Free(reinterpret_cast<uint8*>(mpPtr));

                mpRefCnt = nullptr;
                mpPtr = nullptr;

                return;
            }

            // If the ptr is external, we can assume that it will be freed by the owner.
            if (!mbIsExternalPtr) {
                // The pointer exists but the ref count ptr != the bundled ptr, so we will free the
                // ptr normally.
                gEnginePool->Free<T>(mpPtr);
            }

            mpPtr = nullptr;
        }

        if (mpRefCnt) {
            // Free the ref count
            gEnginePool->Free<FxRefCount>(mpRefCnt);
            mpRefCnt = nullptr;
        }
    }

private:
    /**
     * Decrements the reference count and destroys the object if there are no other references to it.
     */
    void DecRef()
    {
        // Reference count does not exist, we can assume that the object is corrupt or no longer exists.
        if (!mpRefCnt) {
            return;
        }

        // If the reference count is zero, we can free the ptr as it is not being used by any other objects.
        if (mpRefCnt->Dec() == 0) {
#ifdef FX_DEBUG_REF
            FxLogDebug("Destroying FxRef {:p} (RefCnt: {:p})", reinterpret_cast<void*>(mpPtr),
                       reinterpret_cast<void*>(mpRefCnt));
#endif
            // Destroy the reference
            DestroyRef();
        }
    }

    /**
     * Increments the reference count if the reference exists.
     */
    void IncRef()
    {
        if (!mpRefCnt) {
            return;
        }

        mpRefCnt->Inc();
    }


public:
    /** The usage count for the reference */
    FxRefCount* mpRefCnt = nullptr;
    T* mpPtr = nullptr;

    /// Was the memory allocated as one buffer?
    bool mbIsCombinedAllocation : 1 = false;
    bool mbIsExternalPtr : 1 = false;
};
