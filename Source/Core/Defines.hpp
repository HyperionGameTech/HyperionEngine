/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

/// Globally included header - change at your own peril!

#pragma region Compiler and Platform Switches

#if defined(HYP_SHIPPING) && HYP_SHIPPING

#if !defined(HYPERION_BUILD_RELEASE) || !HYPERION_BUILD_RELEASE
#define HYPERION_BUILD_RELEASE 1
#endif

#endif

#if defined(HYPERION_BUILD_RELEASE) && HYPERION_BUILD_RELEASE
#if HYP_DEBUG_MODE
#undef HYP_DEBUG_MODE
#endif // HYP_DEBUG_MODE

#ifdef HYP_ENABLE_CORE_ASSERTIONS
#undef HYP_ENABLE_CORE_ASSERTIONS
#endif // HYP_ENABLE_CORE_ASSERTIONS

#endif // HYPERION_BUILD_RELEASE

#if HYP_CLANG_OR_GCC
#define HYP_PACK_BEGIN __attribute__((__packed__))
#define HYP_PACK_END
#define HYP_FORCE_INLINE __attribute__((always_inline)) inline
#define HYP_USED __attribute__((used))
#endif

#if HYP_MSVC
#define HYP_PACK_BEGIN __pragma(pack(push, 1))
#define HYP_PACK_END __pragma(pack(pop))
#define HYP_FORCE_INLINE __forceinline
#define HYP_USED volatile
#endif

#if HYP_CLANG
#define HYP_DEPRECATED __attribute__((deprecated))
#elif HYP_GCC
#define HYP_DEPRECATED __attribute__((deprecated))
#elif HYP_MSVC
#define HYP_DEPRECATED __declspec(deprecated)
#else
#define HYP_DEPRECATED
#endif

#define HYP_DEPRECATED_BECAUSE(reason) HYP_DEPRECATED

#if HYP_CLANG
#define HYP_NODISCARD __attribute__((warn_unused_result))
#elif HYP_GCC
#define HYP_NODISCARD __attribute__((warn_unused_result))
#elif HYP_MSVC
#define HYP_NODISCARD _Check_return_
#endif

#if HYP_CLANG
#define HYP_NOTNULL __attribute__((nonnull))
#elif HYP_GCC
#define HYP_NOTNULL __attribute__((nonnull))
#elif HYP_MSVC
#define HYP_NOTNULL
#else
#define HYP_NOTNULL
#endif

#if HYP_WINDOWS
#define HYP_FILESYSTEM_SEPARATOR "\\"
#else
#define HYP_FILESYSTEM_SEPARATOR "/"
#endif

#if defined(__arm__) || defined(__aarch64__) || defined(__ARM_ARCH)

#if !defined(HYP_ARM) || !HYP_ARM
#ifdef HYP_ARM
#undef HYP_ARM
#endif
#define HYP_ARM 1
#endif

#endif

#if HYP_APPLE

#include <TargetConditionals.h>

// for Apply Silicon
#if TARGET_CPU_ARM64 && !defined(HYP_ARM) || !HYP_ARM
#ifdef HYP_ARM
#undef HYP_ARM
#endif
#define HYP_ARM 1
#endif

#if (TARGET_IPHONE_SIMULATOR == 1) || (TARGET_OS_IPHONE == 1)

#if !defined(HYP_IOS) || !HYP_IOS
#ifdef HYP_IOS
#undef HYP_IOS
#endif
#define HYP_IOS 1
#endif

#elif (TARGET_OS_OSX == 1)

#if !defined(HYP_MACOS) || !HYP_MACOS
#ifdef HYP_MACOS
#undef HYP_MACOS
#endif
#define HYP_MACOS 1
#endif

#endif
#endif

#if HYP_MSVC
#pragma warning(disable : 4251) // class needs to have dll-interface to be used by clients of class
#pragma warning(disable : 4275) // non dll-interface class used as base for dll-interface class
#endif

#pragma endregion Compiler and Platform Switches

#pragma region Utility Macros

#define HYP_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define HYP_STR(x) #x

#if HYP_WINDOWS
#define HYP_TEXT(x) L##x
#else
#define HYP_TEXT(x) x
#endif

#define HYP_CONCAT(a, b) HYP_CONCAT_INNER(a, b)
#define HYP_CONCAT_INNER(a, b) a##b

#if defined(__cplusplus) && __cplusplus < 202002L
// Fallback for pre-c++20 compilers to "support" consteval.
#define HYP_CONSTEVAL constexpr
// Same for concept, but only usable for `concept bool ...` stuff. Falls back to C++14 constexpr variable templates.
#define HYP_CONCEPT constexpr bool
#define HYP_CONSTEVAL_CONTEXT false
#else
#define HYP_CONSTEVAL consteval
#define HYP_CONCEPT concept
#define HYP_CONSTEVAL_CONTEXT std::is_constant_evaluated()
#endif

// https://mpark.github.io/programming/2017/05/26/constexpr-function-parameters/
#define HYP_MAKE_CONST_ARG(value) \
    [] {                          \
        return (value);           \
    }

#define HYP_GET_CONST_ARG(arg) \
    (arg)()

#define HYP_DEF_STRUCT_COMPARE_EQL(T)                         \
    bool operator==(const T& other) const                     \
    {                                                         \
        return std::memcmp(this, &other, sizeof(*this)) == 0; \
    }                                                         \
    bool operator!=(const T& other) const                     \
    {                                                         \
        return std::memcmp(this, &other, sizeof(*this)) != 0; \
    }

#define HYP_DEF_STRUCT_COMPARE_LT(T)                         \
    bool operator<(const T& other) const                     \
    {                                                        \
        return std::memcmp(this, &other, sizeof(*this)) < 0; \
    }

#define HYP_DEF_STL_ITERATOR(container)        \
    HYP_NODISCARD Iterator Begin()             \
    {                                          \
        return container.begin();              \
    }                                          \
    HYP_NODISCARD Iterator End()               \
    {                                          \
        return container.end();                \
    }                                          \
    HYP_NODISCARD ConstIterator Begin() const  \
    {                                          \
        return container.begin();              \
    }                                          \
    HYP_NODISCARD ConstIterator End() const    \
    {                                          \
        return container.end();                \
    }                                          \
    HYP_NODISCARD Iterator begin()             \
    {                                          \
        return container.begin();              \
    }                                          \
    HYP_NODISCARD Iterator end()               \
    {                                          \
        return container.end();                \
    }                                          \
    HYP_NODISCARD ConstIterator begin() const  \
    {                                          \
        return container.begin();              \
    }                                          \
    HYP_NODISCARD ConstIterator end() const    \
    {                                          \
        return container.end();                \
    }                                          \
    HYP_NODISCARD ConstIterator cbegin() const \
    {                                          \
        return container.cbegin();             \
    }                                          \
    HYP_NODISCARD ConstIterator cend() const   \
    {                                          \
        return container.cend();               \
    }

#define HYP_DEF_STL_BEGIN_END(_begin, _end)   \
    HYP_NODISCARD Iterator Begin()            \
    {                                         \
        return _begin;                        \
    }                                         \
    HYP_NODISCARD Iterator End()              \
    {                                         \
        return _end;                          \
    }                                         \
    HYP_NODISCARD ConstIterator Begin() const \
    {                                         \
        return _begin;                        \
    }                                         \
    HYP_NODISCARD ConstIterator End() const   \
    {                                         \
        return _end;                          \
    }                                         \
    HYP_NODISCARD Iterator begin()            \
    {                                         \
        return _begin;                        \
    }                                         \
    HYP_NODISCARD Iterator end()              \
    {                                         \
        return _end;                          \
    }                                         \
    HYP_NODISCARD ConstIterator begin() const \
    {                                         \
        return _begin;                        \
    }                                         \
    HYP_NODISCARD ConstIterator end() const   \
    {                                         \
        return _end;                          \
    }

#define HYP_DEF_STL_BEGIN_END_CONSTEXPR(_begin, _end)   \
    HYP_NODISCARD constexpr Iterator Begin()            \
    {                                                   \
        return _begin;                                  \
    }                                                   \
    HYP_NODISCARD constexpr Iterator End()              \
    {                                                   \
        return _end;                                    \
    }                                                   \
    HYP_NODISCARD constexpr ConstIterator Begin() const \
    {                                                   \
        return _begin;                                  \
    }                                                   \
    HYP_NODISCARD constexpr ConstIterator End() const   \
    {                                                   \
        return _end;                                    \
    }                                                   \
    HYP_NODISCARD constexpr Iterator begin()            \
    {                                                   \
        return _begin;                                  \
    }                                                   \
    HYP_NODISCARD constexpr Iterator end()              \
    {                                                   \
        return _end;                                    \
    }                                                   \
    HYP_NODISCARD constexpr ConstIterator begin() const \
    {                                                   \
        return _begin;                                  \
    }                                                   \
    HYP_NODISCARD constexpr ConstIterator end() const   \
    {                                                   \
        return _end;                                    \
    }

#define HYP_ENABLE_IF(cond, returnType) \
    typename std::enable_if_t<cond, returnType>

#define HYP_LIKELY(cond) (cond)
#define HYP_UNLIKELY(cond) (cond)

#ifdef HYP_MSVC
#define HYP_DISABLE_OPTIMIZATION __pragma(optimize("", off))
#define HYP_ENABLE_OPTIMIZATION __pragma(optimize("", on))
#elif defined(HYP_CLANG)
#define HYP_DISABLE_OPTIMIZATION _Pragma("clang optimize off")
#define HYP_ENABLE_OPTIMIZATION _Pragma("clang optimize on")
#elif defined(HYP_GCC)
#define HYP_DISABLE_OPTIMIZATION _Pragma("GCC push_options") _Pragma("GCC optimize (\"O0\")")
#define HYP_ENABLE_OPTIMIZATION _Pragma("GCC pop_options")
#endif

#ifdef __COUNTER__
#define HYP_UNIQUE_NAME(prefix) \
    HYP_CONCAT(prefix, __COUNTER__)
#else
#define HYP_UNIQUE_NAME(prefix) \
    prefix
#endif

#define HYP_PAD_STRUCT_HERE(count) \
    uint8 HYP_UNIQUE_NAME(_padding)[count]

#pragma endregion Utility Macros

#pragma region Debug Preprocessor Definitions

#if HYP_DEBUG_MODE
#define HYP_ENABLE_BREAKPOINTS
#endif

#if defined(HYP_CLANG_OR_GCC) && HYP_CLANG_OR_GCC
#define HYP_DEBUG_FUNC_SHORT (__FUNCTION__)
#define HYP_DEBUG_FUNC (__PRETTY_FUNCTION__)
#define HYP_DEBUG_LINE (__LINE__)
#define HYP_FUNCTION_NAME_LIT (__PRETTY_FUNCTION__)

#ifdef HYP_ENABLE_BREAKPOINTS
#ifdef HYP_CLANG
#define HYP_BREAKPOINT __builtin_debugtrap()
#else
#define HYP_BREAKPOINT __builtin_trap()
#endif

namespace Hyperion {
namespace debug {

template <auto FileName, int LineNumber, auto FunctionName>
static HYP_FORCE_INLINE void ExecuteBreakpointOnce()
{
    static struct Impl
    {
        Impl()
        {
            HYP_BREAKPOINT;
        }
    } impl;
}

} // namespace debug
} // namespace Hyperion

#define HYP_BREAKPOINT_ONCE ::Hyperion::debug::ExecuteBreakpointOnce<HYP_STATIC_STRING(__FILE__), __LINE__, HYP_STATIC_STRING(HYP_FUNCTION_NAME_LIT)>()

#endif // HYP_ENABLE_BREAKPOINTS
#elif defined(HYP_MSVC) && HYP_MSVC
#define HYP_DEBUG_FUNC_SHORT (__FUNCTION__)
#define HYP_DEBUG_FUNC (__FUNCSIG__)
#define HYP_DEBUG_LINE (__LINE__)
#define HYP_FUNCTION_NAME_LIT (__FUNCSIG__)

#ifdef HYP_ENABLE_BREAKPOINTS
#define HYP_BREAKPOINT (__debugbreak())
#endif // HYP_ENABLE_BREAKPOINTS

#else // unknown compiler, define empty macros

#define HYP_DEBUG_FUNC_SHORT ""
#define HYP_DEBUG_FUNC ""
#define HYP_DEBUG_LINE (0)
#define HYP_FUNCTION_NAME_LIT ""

#endif

#ifndef HYP_BREAKPOINT
#define HYP_BREAKPOINT (void(0))
#endif

#pragma endregion Debug Preprocessor Definitions

#pragma region Synchonization

#if defined(HYP_MSVC)
#define HYP_WAIT_IDLE() YieldProcessor()
#elif defined(HYP_CLANG_OR_GCC)
#if defined(__x86_64__) || defined(__i386__)
#define HYP_WAIT_IDLE() asm volatile("pause" ::: "memory")
#else
#define HYP_WAIT_IDLE() asm volatile("nop" ::: "memory")
#endif
#endif

// conditionals

#define HYP_ENABLE_THREAD_ID

#ifndef HYP_ENABLE_THREAD_ID
#error "Thread Id is required"
#endif

#if defined(HYP_ENABLE_THREAD_ID) && defined(HYP_DEBUG_MODE)
#define HYP_ENABLE_THREAD_ASSERTIONS
#endif

#pragma endregion Synchonization

#pragma region GPU features

#if HYP_DEBUG_MODE
#if HYP_VULKAN
// #define HYP_VULKAN_DEBUG
#endif
#endif

#if defined(HYP_APPLE) && HYP_APPLE
#ifdef HYP_FEATURES_BINDLESS_TEXTURES
#undef HYP_FEATURES_BINDLESS_TEXTURES
#endif

#ifdef HYP_FEATURES_ENABLE_RAY_TRACING
#undef HYP_FEATURES_ENABLE_RAY_TRACING
#endif

#if defined(HYP_VULKAN) && HYP_VULKAN
#define HYP_VULKAN_API_VERSION VK_API_VERSION_1_2 // moltenvk supports api 1.1
#define HYP_MOLTENVK 1
#endif
#else
#define HYP_FEATURES_ENABLE_RAY_TRACING 1
#define HYP_FEATURES_BINDLESS_TEXTURES 1

#if defined(HYP_VULKAN) && HYP_VULKAN
#define HYP_VULKAN_API_VERSION VK_API_VERSION_1_2
#endif
#endif

#pragma endregion GPU features

#pragma region Memory Management

#ifdef HYP_WINDOWS
#include <malloc.h>
#define HYP_ALLOC_ALIGNED(size, alignment) _aligned_malloc(size, alignment)
#define HYP_FREE_ALIGNED(block) _aligned_free(block)
#else
#include <stdlib.h>
#define HYP_ALLOC_ALIGNED(size, alignment) aligned_alloc(alignment, size)
#define HYP_FREE_ALIGNED(block) free(block)
#endif

#define HYP_ALIGN_ADDRESS(ptr, alignment) (((UIntPtr(ptr) + (alignment) - 1) / (alignment)) * (alignment))
#define HYP_ALIGN_PTR_AS(ptr, T) (std::assume_aligned<alignof(T)>(reinterpret_cast<T*>(HYP_ALIGN_ADDRESS(ptr, alignof(T)))))

#pragma endregion Memory Management

#pragma region Engine Static Configuration

// #define HYP_ENABLE_PROFILE 1

// Disabling compile time Name hashing saves on executable size at the cost of runtime performance
#define HYP_COMPILE_TIME_NAME_HASHING 1

#ifndef HYP_SHIPPING
// uncomment to forego blob storage cache and allow usage
// of *.blob files in non-editor builds.
#define HYP_ALLOW_INLINE_BLOBS 1
#endif // !HYP_SHIPPING

#ifdef HYP_DEBUG_MODE
// #define HYP_ENABLE_MT_CHECK
//  #define HYP_LOG_MEMORY_OPERATIONS

#define HYP_RENDER_COMMANDS_DEBUG_NAME
#endif // HYP_DEBUG_MODE

#ifndef HYP_EDITOR
#define HYP_NO_EDITOR
#endif // !HYP_EDITOR

#ifdef HYP_BULLET
#define HYP_BULLET_PHYSICS 1
#endif // HYP_BULLET

#pragma endregion Engine Static Configuration

#pragma region Symbol Visibility

#ifdef HYP_WINDOWS
#define HYP_EXPORT __declspec(dllexport)
#if defined(HYP_BUILD_STATIC)
#define HYP_IMPORT
#else
#define HYP_IMPORT __declspec(dllimport)
#endif
#elif defined(HYP_CLANG_OR_GCC)
#define HYP_EXPORT __attribute__((visibility("default")))
#define HYP_IMPORT
#else // fallback
#define HYP_EXPORT
#define HYP_IMPORT
#endif

#ifndef HYP_BUILD_CORE_LIBRARY_STATIC
// Modules
#ifdef HYP_BUILD_CORE
#define CORE_API HYP_EXPORT
#else // !HYP_BUILD_CORE
#define CORE_API HYP_IMPORT
#endif // HYP_BUILD_CORE

#else // HYP_BUILD_CORE_LIBRARY_STATIC

#define CORE_API HYP_EXPORT

#endif // !HYP_BUILD_CORE_LIBRARY_STATIC

#ifdef HYP_BUILD_ENGINE
#define ENGINE_API HYP_EXPORT
#else // !HYP_BUILD_ENGINE
#define ENGINE_API HYP_IMPORT
#endif // HYP_BUILD_ENGINE

#ifdef HYP_BUILD_SCRIPT
#define SCRIPT_API HYP_EXPORT
#else // !HYP_BUILD_SCRIPT
#define SCRIPT_API HYP_IMPORT
#endif // HYP_BUILD_SCRIPT

// #ifdef HYP_BUILD_EDITOR
// #define EDITOR_API HYP_EXPORT
// #else // !HYP_BUILD_EDITOR
// #define EDITOR_API HYP_IMPORT
// #endif // HYP_BUILD_EDITOR

// Using ENGINE_API as EDITOR_API temporarily
#define EDITOR_API ENGINE_API

// End modules

#ifdef HYP_TOOL
#define HYP_EXTERN_CLASS static
#else
#if HYP_MSVC
#define HYP_EXTERN_CLASS extern
#else
#define HYP_EXTERN_CLASS ENGINE_API extern
#endif
#endif

#pragma endregion Symbol Visibility

#pragma region Codegen

#define HYP_CLASS(...)
#define HYP_STRUCT(...)
#define HYP_ENUM(...)
#define HYP_METHOD(...)
#define HYP_PROPERTY(name, ...)
#define HYP_FIELD(...)

#pragma endregion
