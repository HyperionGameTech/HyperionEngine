#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <core/debug/Debug.hpp>

#ifdef HYP_LOG_MEMORY_OPERATIONS
    #define VMA_DEBUG_LOG(...)                       \
        {                                            \
            DebugLog(LogType::RenInfo, __VA_ARGS__); \
            puts("");                                \
        }
#endif

#include <system/vma/VmaUsage.hpp>
