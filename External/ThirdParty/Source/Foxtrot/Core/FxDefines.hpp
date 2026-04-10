#pragma once

#define FX_MEMORY_ENGINE_POOL_SIZE (1024ULL * 1024ULL * 300ULL)

////////////////////////////
// Settings
////////////////////////////


#define FX_LOG_OUTPUT_TO_STDOUT
// #define FX_LOG_OUTPUT_TO_FILE
#define FX_LOG_ENABLE_COLORS

#define FX_MEMPOOL_USE_ATOMIC_LOCKING
#define FX_MEMPOOL_TRACK_STATISTICS
#define FX_MEMPOOL_WARN_SLOW_ALLOC
#define FX_MEMPOOL_NEXT_FIT

// #define FX_DEBUG_GPU_BUFFER_LIFETIMES 1

// #define FX_DEBUG_REF

// #define FX_DEBUG_VMA_LOG_ERRORS

// #define FX_SPIN_THREAD_GUARD_DEBUG_USE_MUTEX

// #define FX_DEBUG_GPU_BUFFER_ALLOCATION_NAMES

// #define FX_SIZED_ARRAY_DEBUG
// #define FX_SIZED_ARRAY_NO_MEMPOOL


/// Warn that a paged array has not been called with .Init() before inserting. This can be useful if the default
/// initalization size is too low (leading to more memory allocation calls), or too high (bad fitment of possibly large
/// objects.)
// #define FX_WARN_PAGED_ARRAY_AUTO_INIT 1

// #define FX_DEBUG_IMAGE_VIEWS 1

// #define FX_NO_DEBUG_ASSERTS


////////////////////////////////
// Platform/Compiler macros
////////////////////////////////


#ifdef __APPLE__
#define FX_PLATFORM_MACOS 1
#elif __linux__
#define FX_PLATFORM_LINUX 1
#elif _WIN64
#define FX_PLATFORM_WINDOWS 1
#else
#error "Unsupported platform"
#endif

#ifdef __clang__
#define FX_COMPILER_CLANG 1
#elif __GNUC__
#define FX_COMPILER_GCC 1
#elif _MSC_VER
#define FX_COMPILER_MSVC 1
#else
#error "Unsupported compiler"
#endif

#ifdef FX_NO_SIMD
// FX_NO_SIMD defined
#elif defined __ARM_NEON__ && !defined FX_USE_SIMDE
#define FX_USE_NEON 1
#define FX_USE_SIMD 1

#elif defined __AVX2__ || defined FX_PLATFORM_WINDOWS || defined FX_USE_SIMDE
#define FX_USE_AVX  1
#define FX_USE_SIMD 1
#else
#define FX_NO_SIMD 1
#endif

#ifdef NDEBUG
#define FX_BUILD_RELEASE
#else
#define FX_BUILD_DEBUG
#endif

////////////////////////////////
// Global helper macros
////////////////////////////////

#define FX_SET_MODULE_NAME(str) [[maybe_unused]] static const char* FxModuleName__ = str;


#ifdef _WIN64
#define FX_FORCE_INLINE __forceinline
#else
#define FX_FORCE_INLINE inline __attribute__((always_inline))
#endif

#define FX_ENUM_AS_BITS(EnumType_, value_) static_cast<std::underlying_type_t<EnumType_>>(value_)

#define FX_ENUM_DEFINE_BITWISE_OR(EnumType_)                                                                           \
    inline constexpr EnumType_ operator|(EnumType_ lhs, EnumType_ rhs)                                                 \
    {                                                                                                                  \
        return static_cast<EnumType_>(FX_ENUM_AS_BITS(EnumType_, lhs) | FX_ENUM_AS_BITS(EnumType_, rhs));              \
    }                                                                                                                  \
    inline constexpr EnumType_& operator|=(EnumType_& lhs, EnumType_ rhs)                                              \
    {                                                                                                                  \
        lhs = (lhs | rhs);                                                                                             \
        return lhs;                                                                                                    \
    }

#define FX_ENUM_DEFINE_BITWISE_AND(EnumType_)                                                                          \
    inline constexpr EnumType_ operator&(EnumType_ lhs, EnumType_ rhs)                                                 \
    {                                                                                                                  \
        return static_cast<EnumType_>(FX_ENUM_AS_BITS(EnumType_, lhs) & FX_ENUM_AS_BITS(EnumType_, rhs));              \
    }                                                                                                                  \
    inline constexpr EnumType_& operator&=(EnumType_& lhs, EnumType_ rhs)                                              \
    {                                                                                                                  \
        lhs = (lhs & rhs);                                                                                             \
        return lhs;                                                                                                    \
    }

#define FX_ENUM_DEFINE_BITWISE_NOT(EnumType_)                                                                          \
    inline constexpr EnumType_ operator~(EnumType_ value)                                                              \
    {                                                                                                                  \
        return static_cast<EnumType_>(~FX_ENUM_AS_BITS(EnumType_, value));                                             \
    }


#define FX_DEFINE_ENUM_AS_FLAGS(EnumType_)                                                                             \
    FX_ENUM_DEFINE_BITWISE_OR(EnumType_)                                                                               \
    FX_ENUM_DEFINE_BITWISE_AND(EnumType_)                                                                              \
    FX_ENUM_DEFINE_BITWISE_NOT(EnumType_)                                                                              \
    using EnumType_##Bits = std::underlying_type_t<EnumType_>;
