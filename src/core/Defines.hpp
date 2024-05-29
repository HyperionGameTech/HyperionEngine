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

/// Support for checking if VA_OPT is available:
// https://stackoverflow.com/a/48045656
#define HYP_PP_THIRD_ARG(a,b,c,...) c
#define HYP_VA_OPT_SUPPORTED_I(...) HYP_PP_THIRD_ARG(__VA_OPT__(,),true,false,)
#define HYP_VA_OPT_SUPPORTED HYP_VA_OPT_SUPPORTED_I(?)

#pragma endregion Compiler and Platform Switches

#pragma region Utility Macros

#define HYP_STR(x) #x
#define HYP_METHOD(method) HYP_STR(method)

#ifdef HYP_WINDOWS
#define HYP_TEXT(x) L##x
#else
#define HYP_TEXT(x) x
#endif

#define HYP_CONCAT(a, b) HYP_CONCAT_INNER(a, b)
#define HYP_CONCAT_INNER(a, b) a ## b

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
    [[nodiscard]] Iterator Begin()             { return container.begin(); }  \
    [[nodiscard]] Iterator End()               { return container.end(); }    \
    [[nodiscard]] ConstIterator Begin() const  { return container.begin(); }  \
    [[nodiscard]] ConstIterator End() const    { return container.end(); }    \
    [[nodiscard]] Iterator begin()             { return container.begin(); }  \
    [[nodiscard]] Iterator end()               { return container.end(); }    \
    [[nodiscard]] ConstIterator begin() const  { return container.begin(); }  \
    [[nodiscard]] ConstIterator end() const    { return container.end(); }    \
    [[nodiscard]] ConstIterator cbegin() const { return container.cbegin(); } \
    [[nodiscard]] ConstIterator cend() const   { return container.cend(); }

#define HYP_DEF_STL_BEGIN_END(_begin, _end) \
    [[nodiscard]] Iterator Begin()             { return _begin; } \
    [[nodiscard]] Iterator End()               { return _end; }   \
    [[nodiscard]] ConstIterator Begin() const  { return _begin; } \
    [[nodiscard]] ConstIterator End() const    { return _end; }   \
    [[nodiscard]] Iterator begin()             { return _begin; } \
    [[nodiscard]] Iterator end()               { return _end; }   \
    [[nodiscard]] ConstIterator begin() const  { return _begin; } \
    [[nodiscard]] ConstIterator end() const    { return _end; }

#define HYP_ENABLE_IF(cond, return_type) \
    typename std::enable_if_t<cond, return_type>

#define HYP_LIKELY(cond) cond
#define HYP_UNLIKELY(cond) cond

#ifdef HYP_MSVC
#define HYP_DISABLE_OPTIMIZATION __pragma(optimize("", off))
#define HYP_ENABLE_OPTIMIZATION __pragma(optimize("", on))
#elif defined(HYP_CLANG_OR_GCC)
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

#if !defined(HYPERION_BUILD_RELEASE) || !HYPERION_BUILD_RELEASE
    #define HYP_DEBUG_MODE
#endif

#if defined(HYPERION_BUILD_RELEASE_FINAL) && HYPERION_BUILD_RELEASE_FINAL
    #define HYP_ENABLE_BREAKPOINTS 0
#else
    #define HYP_ENABLE_BREAKPOINTS 1
#endif

#if defined(HYP_CLANG_OR_GCC) && HYP_CLANG_OR_GCC
    #define HYP_DEBUG_FUNC_SHORT __FUNCTION__
    #define HYP_DEBUG_FUNC       __PRETTY_FUNCTION__
    #define HYP_DEBUG_LINE       (__LINE__)

    #if HYP_ENABLE_BREAKPOINTS
        #if (defined(HYP_ARM) && HYP_ARM) || HYP_GCC
            #define HYP_BREAKPOINT { __builtin_trap(); }
        #else
            #define HYP_BREAKPOINT { __asm__ volatile("int $0x03"); }
        #endif
    #endif
#elif defined(HYP_MSVC) && HYP_MSVC
    #define HYP_DEBUG_FUNC_SHORT (__FUNCTION__)
    #define HYP_DEBUG_FUNC       (__FUNCSIG__)
    #define HYP_DEBUG_LINE       (__LINE__)
    #if HYP_ENABLE_BREAKPOINTS
        #define HYP_BREAKPOINT       (__debugbreak())
    #endif
#else
    #define HYP_DEBUG_FUNC_SHORT ""
    #define HYP_DEBUG_FUNC ""
    #define HYP_DEBUG_LINE (0)
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

#pragma endregion Debugging

#pragma region Error Handling

#if defined(HYP_USE_EXCEPTIONS) && HYP_USE_EXCEPTIONS
    #define HYP_THROW(msg) throw ::std::runtime_error(msg)
#else
    #ifdef HYP_DEBUG_MODE
        #define HYP_THROW(msg) \
            do { \
                HYP_BREAKPOINT; \
                std::terminate(); \
            } while (0)
    #else
        #define HYP_THROW(msg) std::terminate()
    #endif
#endif

#pragma endregion Error Handling

#pragma region Synchonization

#define HYP_WAIT_IDLE() \
    do { \
        volatile int x = 0; \
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

#define HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA

#define HYP_FEATURES_PARALLEL_RENDERING 1

// Disabling compile time Name hashing saves on executable size at the cost of runtime performance
#define HYP_COMPILE_TIME_NAME_HASHING 1

// #define HYP_LOG_MEMORY_OPERATIONS
// #define HYP_LOG_DESCRIPTOR_SET_UPDATES
// #define HYP_DEBUG_LOG_RENDER_COMMANDS

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

#endif // !HYPERION_DEFINES_HPP
