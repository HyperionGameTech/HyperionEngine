// #pragma once

// #include "FxTypes.hpp"

// #include <concepts>

// /**
//  * Concept to determine if a type is an allocator
//  */
// template <typename T>
// concept C_IsAllocator = requires() {
//     { T::Alloc(0xB00B) };
//     { T::Free(nullptr) };
// };

// /**
//  * Allocates and frees memory using `new` and `delete`.
//  */
// template <typename T>
// class FxCppAllocator
// {
// public:
//     template <typename... Args>
//     static T* Alloc(uint32 size, Args... args)
//     {
//         return new T[size];
//     }

//     static void Free(T* ptr) { delete[] ptr; }
// };

// #include "FxMemory.hpp"

// /**
//  * Allocated and frees memory on the global memory pool
//  * @see FxMemPool
//  */
// template <typename T>
// class FxGlobalPoolAllocator
// {
// public:
//     template <typename... Args>
//     static T* Alloc(uint32 size, Args... args)
//     {
//         return FxMemPool::Alloc<T>(size, std::forward<Args>(args)...);
//     }

//     static void Free(T* ptr) { FxMemPool::Free<T>(ptr); }
// };
