#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_ASSERT_LEAK(...)

#include <Core/logging/Logger.hpp>

// namespace Hyperion {
// HYP_DECLARE_LOG_CHANNEL(RenderingBackend);
// HYP_DEFINE_LOG_SUBCHANNEL(VulkanMemoryAllocator, RenderingBackend);
// } // namespace Hyperion

// using namespace Hyperion;

#ifdef HYP_LOG_MEMORY_OPERATIONS
#define VMA_DEBUG_LOG(...)                       \
    {                                            \
        DebugLog(LogType::RenInfo, __VA_ARGS__); \
        puts("");                                \
    }
#endif

#include <vma/vk_mem_alloc.h>
