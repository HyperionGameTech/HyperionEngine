#pragma once

/*
 * Heavily modified version of the TLSF memory allocator v3.1 written by Matthew Conte.
 * See FxMemPool.cpp for the original license.
 */

#include <stddef.h>

#include <Core/FxTypes.hpp>
#include <mutex>

typedef void* tlsf_t;
typedef void* pool_t;

class ControlBlock;

class FxMemPool
{
public:
    using WalkerFunc = void (*)(void* ptr, size_t size, int32 used, void* user);

public:
    using Status = int32;
    static constexpr Status scPoolOk = 0;

    FxMemPool() = default;

    void Create(uint64 size);

    tlsf_t CreateFromPtr(void* allocated_buffer);
    tlsf_t CreateWithPool(void* mem, size_t bytes);

    pool_t AddPool(void* mem, size_t bytes);
    void RemovePool(pool_t pool);
    pool_t GetPool();

    void* AllocRaw(size_t bytes);
    void* AlignedAllocRaw(uint32 alignment, size_t bytes);
    void* ReallocRaw(void* ptr, size_t size);
    void FreeRaw(void* ptr);

    template <typename T, typename... TArgs>
    T* Alloc(uint32 size, TArgs&&... args)
    {
        T* ptr = static_cast<T*>(AllocRaw(size));

        if constexpr (std::is_constructible_v<T, TArgs...>) {
            ::new (ptr) T(std::forward<TArgs>(args)...);
        }

        return ptr;
    }

    template <typename T>
    T* Realloc(T* ptr, uint32 size)
    {
        T* new_ptr = static_cast<T*>(ReallocRaw(ptr, size));

        return new_ptr;
    }

    template <typename T>
    void Free(T* ptr)
    {
        if constexpr (std::is_destructible_v<T>) {
            ptr->~T();
        }

        FreeRaw(reinterpret_cast<void*>(ptr));
    }


    /* Debugging. */
    void WalkPool(pool_t pool, WalkerFunc walker, void* user);
    Status CheckIntegrity();
    Status CheckPool(pool_t pool);

    void Destroy();

    ~FxMemPool() { Destroy(); }

private:
    ControlBlock* GetControlBlock();

public:
    void* pMemory;

    std::mutex mMutex;
};
