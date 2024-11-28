/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEFINES_HPP
#define HYPERION_DEFINES_HPP

#pragma region Compiler and Platform Switches

#if defined(HYPERION_BUILD_RELEASE_FINAL) && HYPERION_BUILD_RELEASE_FINAL
    #define HYPERION_BUILD_RELEASE 1 // just to ensure
#endif

#if defined(__GNUC__) || defined(__GNUG__)
    #define HYP_GCC 1
#endif

#if defined(__clang__)
    #define HYP_CLANG 1
#endif

#if defined(HYP_GCC) || defined(HYP_CLANG)
    #define HYP_CLANG_OR_GCC 1
#elif defined(_MSC_VER)
    #define HYP_MSVC 1
#else
    #error Unknown compiler
#endif

#if defined(HYP_CLANG_OR_GCC) && HYP_CLANG_OR_GCC
    #define HYP_PACK_BEGIN __attribute__((__packed__))
    #define HYP_PACK_END
    #define HYP_FORCE_INLINE __attribute__((always_inline))
    #define HYP_USED __attribute__((used))
#endif

#if defined(HYP_MSVC) && HYP_MSVC
    #define HYP_PACK_BEGIN __pragma(pack(push, 1))
    #define HYP_PACK_END __pragma(pack(pop))
    #define HYP_FORCE_INLINE __forceinline
    #define HYP_USED volatile
#endif

#if defined(HYP_CLANG) && HYP_CLANG
    #define HYP_DEPRECATED __attribute__((deprecated))
#elif defined(HYP_GCC) && HYP_GCC
    #define HYP_DEPRECATED __attribute__((deprecated))
#elif defined(HYP_MSVC) && HYP_MSVC
    #define HYP_DEPRECATED __declspec(deprecated)
#endif

#if defined(HYP_CLANG) && HYP_CLANG
    #define HYP_NODISCARD __attribute__((warn_unused_result))
#elif defined(HYP_GCC) && HYP_GCC
    #define HYP_NODISCARD __attribute__((warn_unused_result))
#elif defined(HYP_MSVC) && HYP_MSVC
    #define HYP_NODISCARD _Check_return_
#endif

#if defined(HYP_CLANG) && HYP_CLANG
    #define HYP_NOTNULL __attribute__((nonnull))
#elif defined(HYP_GCC) && HYP_GCC
    #define HYP_NOTNULL __attribute__((nonnull))
#elif defined(HYP_MSVC) && HYP_MSVC
    #define HYP_NOTNULL 
#else
    #define HYP_NOTNULL
#endif

#if defined(_WIN32) && _WIN32
    #define HYP_WINDOWS 1

    #define HYP_FILESYSTEM_SEPARATOR "\\"
#else
    #define HYP_FILESYSTEM_SEPARATOR "/"
#endif

#if defined(unix) || defined(__unix) || defined(__unix__)
    #define HYP_UNIX 1
#endif

#if defined(__linux__) || defined(linux) || defined(__linux)
    #define HYP_LINUX 1
#endif

#ifdef __arm__
    #define HYP_ARM 1
#endif

#ifdef __APPLE__
    #define HYP_UNIX 1
    #define HYP_APPLE 1

    #include <TargetConditionals.h>

    #ifndef HYP_ARM
        // for m1
        #if TARGET_CPU_ARM64
            #define HYP_ARM 1
        #endif
    #endif

    #if (TARGET_IPHONE_SIMULATOR == 1) || (TARGET_OS_IPHONE == 1)
        #define HYP_IOS 1
    #elif (TARGET_OS_OSX == 1)
        #define HYP_MACOS 1
    #endif
#endif

#ifdef __linux__
    #define HYP_LINUX 1

    #ifndef HYP_UNIX
        #define HYP_UNIX 1
    #endif
#endif

#ifdef HYP_MSVC
#pragma warning( disable : 4251 ) // class needs to have dll-interface to be used by clients of class
#pragma warning( disable : 4275 ) // non dll-interface class used as base for dll-interface class
#endif

#pragma endregion Compiler and Platform Switches

#pragma region Utility Macros

#define HYP_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define HYP_STR(x) #x

#ifdef HYP_WINDOWS
#define HYP_TEXT(x) L##x
#else
#define HYP_TEXT(x) x
#endif

#define HYP_CONCAT(a, b) HYP_CONCAT_INNER(a, b)
#define HYP_CONCAT_INNER(a, b) a ## b

// https://mpark.github.io/programming/2017/05/26/constexpr-function-parameters/
#define HYP_MAKE_CONST_ARG(value) \
    [] { return (value); }

#define HYP_GET_CONST_ARG(arg) \
    (arg)()

#define HYP_DEF_STRUCT_COMPARE_EQL(hyp_class) \
    bool operator==(const hyp_class &other) const { \
        return std::memcmp(this, &other, sizeof(*this)) == 0; \
    } \
    bool operator!=(const hyp_class &other) const { \
        return std::memcmp(this, &other, sizeof(*this)) != 0; \
    }

#define HYP_DEF_STRUCT_COMPARE_LT(hyp_class) \
    bool operator<(const hyp_class &other) const { \
        return std::memcmp(this, &other, sizeof(*this)) < 0; \
    }

#define HYP_DEF_STL_HASH(hyp_class) \
    template<> \
    struct std::hash<hyp_class> { \
        size_t operator()(const hyp_class &obj) const \
        { \
            return obj.GetHashCode().Value(); \
        } \
    }

#define HYP_DEF_STL_ITERATOR(container) \
    HYP_NODISCARD Iterator Begin()             { return container.begin(); }  \
    HYP_NODISCARD Iterator End()               { return container.end(); }    \
    HYP_NODISCARD ConstIterator Begin() const  { return container.begin(); }  \
    HYP_NODISCARD ConstIterator End() const    { return container.end(); }    \
    HYP_NODISCARD Iterator begin()             { return container.begin(); }  \
    HYP_NODISCARD Iterator end()               { return container.end(); }    \
    HYP_NODISCARD ConstIterator begin() const  { return container.begin(); }  \
    HYP_NODISCARD ConstIterator end() const    { return container.end(); }    \
    HYP_NODISCARD ConstIterator cbegin() const { return container.cbegin(); } \
    HYP_NODISCARD ConstIterator cend() const   { return container.cend(); }

#define HYP_DEF_STL_BEGIN_END(_begin, _end) \
    HYP_NODISCARD Iterator Begin()             { return _begin; } \
    HYP_NODISCARD Iterator End()               { return _end; }   \
    HYP_NODISCARD ConstIterator Begin() const  { return _begin; } \
    HYP_NODISCARD ConstIterator End() const    { return _end; }   \
    HYP_NODISCARD Iterator begin()             { return _begin; } \
    HYP_NODISCARD Iterator end()               { return _end; }   \
    HYP_NODISCARD ConstIterator begin() const  { return _begin; } \
    HYP_NODISCARD ConstIterator end() const    { return _end; }

#define HYP_DEF_STL_BEGIN_END_CONSTEXPR(_begin, _end) \
    HYP_NODISCARD constexpr Iterator Begin()             { return _begin; } \
    HYP_NODISCARD constexpr Iterator End()               { return _end; }   \
    HYP_NODISCARD constexpr ConstIterator Begin() const  { return _begin; } \
    HYP_NODISCARD constexpr ConstIterator End() const    { return _end; }   \
    HYP_NODISCARD constexpr Iterator begin()             { return _begin; } \
    HYP_NODISCARD constexpr Iterator end()               { return _end; }   \
    HYP_NODISCARD constexpr ConstIterator begin() const  { return _begin; } \
    HYP_NODISCARD constexpr ConstIterator end() const    { return _end; }

#define HYP_ENABLE_IF(cond, return_type) \
    typename std::enable_if_t<cond, return_type>

#define HYP_LIKELY(cond) cond
#define HYP_UNLIKELY(cond) cond

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

#define HYP_PAD_STRUCT_HERE(type, count) \
    type HYP_UNIQUE_NAME(_padding)[count]

#pragma endregion Utility Macros

#pragma region Debugging

// #define HYP_USE_EXCEPTIONS

#if defined(HYPERION_BUILD_RELEASE_FINAL) && HYPERION_BUILD_RELEASE_FINAL
    #define HYP_ENABLE_BREAKPOINTS 0
#else
    #define HYP_ENABLE_BREAKPOINTS 1
#endif

#if defined(HYP_CLANG_OR_GCC) && HYP_CLANG_OR_GCC
    #define HYP_DEBUG_FUNC_SHORT    (__FUNCTION__)
    #define HYP_DEBUG_FUNC          (__PRETTY_FUNCTION__)
    #define HYP_DEBUG_LINE          (__LINE__)
    #define HYP_FUNCTION_NAME_LIT   (__PRETTY_FUNCTION__)

    #if HYP_ENABLE_BREAKPOINTS
        #if (defined(HYP_ARM) && HYP_ARM) || HYP_GCC
            #define HYP_BREAKPOINT { __builtin_trap(); }
        #else
            #define HYP_BREAKPOINT { __asm__ volatile("int $0x03"); }
        #endif
    #endif
#elif defined(HYP_MSVC) && HYP_MSVC
    #define HYP_DEBUG_FUNC_SHORT    (__FUNCTION__)
    #define HYP_DEBUG_FUNC          (__FUNCSIG__)
    #define HYP_DEBUG_LINE          (__LINE__)
    #define HYP_FUNCTION_NAME_LIT   (__FUNCSIG__)

    #if HYP_ENABLE_BREAKPOINTS
        #define HYP_BREAKPOINT (__debugbreak())
    #endif
#else
    #define HYP_DEBUG_FUNC_SHORT ""
    #define HYP_DEBUG_FUNC ""
    #define HYP_DEBUG_LINE (0)
    #define HYP_FUNCTION_NAME_LIT ""

    #if HYP_ENABLE_BREAKPOINTS
        #define HYP_BREAKPOINT       (void(0))
    #endif
#endif

#ifdef HYP_DEBUG_MODE
    #define HYP_BREAKPOINT_DEBUG_MODE HYP_BREAKPOINT
#else
    #define HYP_BREAK_IF_DEBUG_MODE  (void(0))
#endif

#if !defined(HYP_ENABLE_BREAKPOINTS) || !HYP_ENABLE_BREAKPOINTS
    #define HYP_BREAKPOINT           (void(0))
#endif

#ifdef HYP_DEBUG_MODE
    #define HYP_PRINT_STACK_TRACE() \
        ::hyperion::LogStackTrace()
#else
    #define HYP_PRINT_STACK_TRACE()
#endif

#pragma endregion Debugging

#pragma region Error Handling

#if defined(HYP_USE_EXCEPTIONS) && HYP_USE_EXCEPTIONS
    #define HYP_THROW(msg) throw ::std::runtime_error(msg)
#else
    #ifdef HYP_DEBUG_MODE
        #define HYP_THROW(msg) \
            do { \
                HYP_PRINT_STACK_TRACE(); \
                HYP_BREAKPOINT; \
                std::terminate(); \
            } while (0)
    #else
        #define HYP_THROW(msg) std::terminate()
    #endif
#endif

#define HYP_UNREACHABLE() \
    HYP_THROW("Unreachable code hit in function " HYP_DEBUG_FUNC)

#define HYP_NOT_IMPLEMENTED() \
    HYP_THROW("Function not implemented: " HYP_DEBUG_FUNC); \
    return { }

#define HYP_NOT_IMPLEMENTED_VOID() \
    HYP_THROW("Function not implemented: " HYP_DEBUG_FUNC); \
    return

#pragma endregion Error Handling

#pragma region Synchonization

#define HYP_WAIT_IDLE() \
    do { \
        volatile uint32 x = 0; \
        x = x + 1; \
    } while (0)

// conditionals

#define HYP_ENABLE_THREAD_ID

#ifndef HYP_ENABLE_THREAD_ID
    #error "Thread ID is required"
#endif

#if defined(HYP_ENABLE_THREAD_ID) && defined(HYP_DEBUG_MODE)
    #define HYP_ENABLE_THREAD_ASSERTIONS
#endif

#pragma endregion Synchonization

#pragma region GPU features

#ifdef HYP_DEBUG_MODE
    #ifdef HYP_VULKAN
        //#define HYP_VULKAN_DEBUG
    #endif
#endif

#if defined(HYP_APPLE) && HYP_APPLE
    #ifdef HYP_FEATURES_BINDLESS_TEXTURES
        #undef HYP_FEATURES_BINDLESS_TEXTURES
    #endif

    #ifdef HYP_FEATURES_ENABLE_RAYTRACING
        #undef HYP_FEATURES_ENABLE_RAYTRACING
    #endif

    #if defined(HYP_VULKAN) && HYP_VULKAN
        #define HYP_VULKAN_API_VERSION VK_API_VERSION_1_1 // moltenvk supports api 1.1
        #define HYP_MOLTENVK 1

        #ifdef HYP_DEBUG_MODE
            //#define HYP_MOLTENVK_LINKED 1
        #endif
    #endif
#else
    #define HYP_FEATURES_ENABLE_RAYTRACING 1
    #define HYP_FEATURES_BINDLESS_TEXTURES 1

    #if defined(HYP_VULKAN) && HYP_VULKAN
        #define HYP_VULKAN_API_VERSION VK_API_VERSION_1_2
    #endif
#endif

#pragma endregion GPU features

#pragma region Memory Management

#ifdef HYP_WINDOWS
    #include <malloc.h>
    #define HYP_ALLOC_ALIGNED(alignment, size) _aligned_malloc(size, alignment)
    #define HYP_FREE_ALIGNED(block) _aligned_free(block)
#else
    #include <stdlib.h>
    #define HYP_ALLOC_ALIGNED(alignment, size) aligned_alloc(alignment, size)
    #define HYP_FREE_ALIGNED(block) free(block)
#endif

#pragma endregion Memory Management

#pragma region Engine Static Configuration

#define HYP_FEATURES_PARALLEL_RENDERING 1
#define HYP_ENABLE_PROFILE

// Disabling compile time Name hashing saves on executable size at the cost of runtime performance
#define HYP_COMPILE_TIME_NAME_HASHING 1

#ifdef HYP_DEBUG_MODE
    #define HYP_ENABLE_MT_CHECK
    // #define HYP_LOG_MEMORY_OPERATIONS
    // #define HYP_LOG_DESCRIPTOR_SET_UPDATES

    #define HYP_RENDER_COMMANDS_DEBUG_NAME

    // Add more data to RefCountedPtr to track down memory leaks.
    // Expensive as it requires a mutex lock on every ref count operation, so only enable in debugging.
    // #define HYP_ENABLE_REF_TRACKING
#endif

#if !defined(HYP_EDITOR) || !HYP_EDITOR
    #define HYP_NO_EDITOR
#endif

#if defined(HYP_BULLET) && HYP_BULLET
    #define HYP_BULLET_PHYSICS 1
#endif

#pragma endregion Engine Static Configuration

#pragma region Symbol Visibility

#ifdef HYP_MSVC
#define HYP_EXPORT __declspec(dllexport)
#define HYP_IMPORT __declspec(dllimport)
#elif defined(HYP_CLANG_OR_GCC)
#define HYP_EXPORT __attribute__((visibility("default")))
#define HYP_IMPORT
#endif

#ifdef HYP_BUILD_LIBRARY
#define HYP_API HYP_EXPORT
#define HYP_C_API extern "C" HYP_EXPORT
#else
#define HYP_API HYP_IMPORT
#define HYP_C_API extern "C" HYP_IMPORT
#endif

#pragma endregion Symbol Visibility

#pragma region Global Forward Declarations

namespace hyperion {

extern HYP_API void LogStackTrace(int depth = 10);

} // namespace hyperion

#pragma endregion Global Forward Declarations

#pragma region Codegen

#define HYP_CLASS(...)
#define HYP_STRUCT(...)
#define HYP_ENUM(...)
#define HYP_METHOD(...)
#define HYP_PROPERTY(name, ...)
#define HYP_FIELD(...)

#pragma endregion

#endif // !HYPERION_DEFINES_HPP
