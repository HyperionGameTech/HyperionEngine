/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/math/MathUtil.hpp>

#include <engine/EngineDriver.hpp>

#include <cstring>

#include <VulkanGpuBuffer.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface RI;

#pragma region Helpers

static uint32 FindMemoryType(uint32 vkTypeFilter, VkMemoryPropertyFlags vkMemoryPropertyFlags)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(RI.GetDevice()->GetPhysicalDevice(), &memProperties);

    for (uint32 i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((vkTypeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & vkMemoryPropertyFlags) == vkMemoryPropertyFlags)
        {
            HYP_LOG(RenderingBackend, Verbose, "Found Memory type {}", i);
            return i;
        }
    }

    HYP_FAIL("Could not find suitable memory type!");
}

#pragma endregion Helpers

#pragma region VulkanGpuBuffer

VulkanGpuBuffer::VulkanGpuBuffer(GpuBufferType type, size_t size, size_t alignment)
    : GpuBufferBase(type, size, alignment)
{
}

VulkanGpuBuffer::VulkanGpuBuffer(VulkanGpuBuffer&& other) noexcept
    : GpuBufferBase(other.m_type, other.m_size, other.m_alignment),
      m_handle(other.m_handle),
      m_vmaAllocation(other.m_vmaAllocation),
      m_mapping(other.m_mapping)
{
    other.m_handle = VK_NULL_HANDLE;
    other.m_vmaAllocation = VK_NULL_HANDLE;
    other.m_mapping = nullptr;
    other.m_resourceState = RS_UNDEFINED;
}

VulkanGpuBuffer& VulkanGpuBuffer::operator=(VulkanGpuBuffer&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    // Clean up existing buffer if it exists
    if (IsCreated())
    {
        if (m_mapping != nullptr)
        {
            Unmap();
        }

        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle, allocation = m_vmaAllocation]() -> void
            {
                vmaDestroyBuffer(RI.GetDevice()->GetVmaAllocator(), handle, allocation);
            }));
    }

    m_type = other.m_type;
    m_size = other.m_size;
    m_alignment = other.m_alignment;
    m_resourceState = other.m_resourceState;
    m_handle = other.m_handle;
    m_vmaAllocation = other.m_vmaAllocation;
    m_mapping = other.m_mapping;

    other.m_handle = VK_NULL_HANDLE;
    other.m_vmaAllocation = VK_NULL_HANDLE;
    other.m_mapping = nullptr;
    other.m_resourceState = RS_UNDEFINED;

    return *this;
}

VulkanGpuBuffer::~VulkanGpuBuffer()
{
    if (!IsCreated())
    {
        return;
    }

    if (m_mapping != nullptr)
    {
        Unmap();
    }

    EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle, allocation = m_vmaAllocation]() -> void
        {
            vmaDestroyBuffer(RI.GetDevice()->GetVmaAllocator(), handle, allocation);
        }));

    m_handle = VK_NULL_HANDLE;
    m_vmaAllocation = VK_NULL_HANDLE;
    m_resourceState = RS_UNDEFINED;
}

void VulkanGpuBuffer::Memset(size_t count, ubyte value)
{
    if (m_mapping == nullptr)
    {
        Map();
    }

    Memory::Fill(m_mapping, value, count);
}

void VulkanGpuBuffer::Copy(size_t count, const void* ptr)
{
    if (m_mapping == nullptr)
    {
        Map();
    }

    Memory::Copy(m_mapping, ptr, count);
}

void VulkanGpuBuffer::Copy(size_t offset, size_t count, const void* ptr)
{
    if (m_mapping == nullptr)
    {
        Map();
    }

    AssertDebug(offset + count <= m_size);

    Memory::Copy(reinterpret_cast<void*>(UIntPtr(m_mapping) + offset), ptr, count);
}

void VulkanGpuBuffer::Read(size_t count, void* outPtr) const
{
    if (m_mapping == nullptr)
    {
        Map();

        HYP_LOG(RenderingBackend, Warning, "Attempt to Read() from buffer but data has not been mapped previously");
    }

    AssertDebug(count <= m_size);

    Memory::Copy(outPtr, m_mapping, count);
}

void VulkanGpuBuffer::Read(size_t offset, size_t count, void* outPtr) const
{
    if (m_mapping == nullptr)
    {
        Map();

        HYP_LOG(RenderingBackend, Warning, "Attempt to Read() from buffer but data has not been mapped previously");
    }

    AssertDebug(offset + count <= m_size);

    Memory::Copy(outPtr, reinterpret_cast<void*>(UIntPtr(m_mapping) + UIntPtr(offset)), count);
}

void* VulkanGpuBuffer::Map() const
{
    if (m_mapping != nullptr)
    {
        return m_mapping;
    }

    Assert(IsCpuAccessible(), "Attempt to map a buffer that is not CPU accessible!");

    vmaMapMemory(RI.GetDevice()->GetVmaAllocator(), m_vmaAllocation, &m_mapping);

    return m_mapping;
}

void VulkanGpuBuffer::Unmap() const
{
    if (m_mapping == nullptr)
    {
        return;
    }

    vmaUnmapMemory(RI.GetDevice()->GetVmaAllocator(), m_vmaAllocation);
    m_mapping = nullptr;
}

void VulkanGpuBuffer::Flush(size_t offset, size_t count)
{
    if (!IsCreated())
    {
        return;
    }

    AssertDebug(offset + count <= Size());

    VkResult result = vmaFlushAllocation(RI.GetDevice()->GetVmaAllocator(), m_vmaAllocation, offset, count);
    Assert(result == VK_SUCCESS);
}

bool VulkanGpuBuffer::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

bool VulkanGpuBuffer::IsCpuAccessible() const
{
    VmaAllocationInfo info {};
    vmaGetAllocationInfo(RI.GetDevice()->GetVmaAllocator(), m_vmaAllocation, &info);

    VkMemoryPropertyFlags flags = 0;
    vmaGetMemoryTypeProperties(RI.GetDevice()->GetVmaAllocator(), info.memoryType, &flags);

    return (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
}

RendererResult VulkanGpuBuffer::CheckCanAllocate(size_t size) const
{
    const VkBufferCreateInfo createInfo = GetBufferCreateInfo();
    const VmaAllocationCreateInfo allocInfo = GetAllocationCreateInfo();

    return CheckCanAllocate(createInfo, allocInfo, m_size);
}

uint64 VulkanGpuBuffer::GetBufferDeviceAddress() const
{
    Assert(
        RI.GetDevice()->GetFeatures().GetBufferDeviceAddressFeatures().bufferDeviceAddress,
        "Called GetBufferDeviceAddress() but the buffer device address extension feature is not supported or enabled!");

    Assert(m_handle != VK_NULL_HANDLE);

    VkBufferDeviceAddressInfoKHR info { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = m_handle;

    return RI.dynamicFunctions.vkGetBufferDeviceAddressKHR(
        RI.GetDevice()->GetDevice(),
        &info);
}

void VulkanGpuBuffer::InsertBarrier(
    VulkanCommandBuffer* commandBuffer,
    ResourceState newState) const
{
    AssertDebug(!commandBuffer->IsInRenderPass());

    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to insert a resource barrier but buffer was not created");

        return;
    }

    VkBufferMemoryBarrier barrier { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = GetVkAccessMask(m_resourceState, false);
    barrier.dstAccessMask = GetVkAccessMask(newState, false);
    barrier.buffer = m_handle;
    barrier.offset = 0;
    barrier.size = m_size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        commandBuffer->GetVulkanHandle(),
        GetVkShaderStageMask(m_resourceState, true, false),
        GetVkShaderStageMask(newState, false, false),
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr);

    m_resourceState = newState;
}

void VulkanGpuBuffer::InsertBarrier(
    VulkanCommandBuffer* commandBuffer,
    ResourceState newState,
    ShaderModuleType shaderType) const
{
    AssertDebug(!commandBuffer->IsInRenderPass());

    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to insert a resource barrier but buffer was not created");

        return;
    }

    VkBufferMemoryBarrier barrier { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = GetVkAccessMask(m_resourceState, false);
    barrier.dstAccessMask = GetVkAccessMask(newState, false);
    barrier.buffer = m_handle;
    barrier.offset = 0;
    barrier.size = m_size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        commandBuffer->GetVulkanHandle(),
        GetVkShaderStageMask(m_resourceState, true, false, shaderType),
        GetVkShaderStageMask(newState, false, false, shaderType),
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr);

    m_resourceState = newState;
}

void VulkanGpuBuffer::CopyFrom(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuBuffer* srcBuffer,
    uint32 count)
{
    AssertDebug(!commandBuffer->IsInRenderPass());

    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but dst buffer was not created");

        return;
    }

    if (!srcBuffer->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but src buffer was not created");

        return;
    }

    AssertDebug(srcBuffer->GetBufferUsageFlags() & VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    AssertDebug(GetBufferUsageFlags() & VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    VkBufferCopy region {};
    region.size = count;

    vkCmdCopyBuffer(
        commandBuffer->GetVulkanHandle(),
        srcBuffer->m_handle,
        m_handle,
        1,
        &region);
}

void VulkanGpuBuffer::CopyFrom(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuBuffer* srcBuffer,
    uint32 srcOffset, uint32 dstOffset,
    uint32 count)
{
    AssertDebug(!commandBuffer->IsInRenderPass());

    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but dst buffer was not created");

        return;
    }

    if (!srcBuffer->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but src buffer was not created");

        return;
    }

    Assert((srcOffset + count <= srcBuffer->Size()) && (dstOffset + count <= Size()), "Copy out of bounds!");

    //AssertDebug(srcBuffer->GetBufferUsageFlags() & VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    //AssertDebug(GetBufferUsageFlags() & VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    VkBufferCopy region {};
    region.size = count;
    region.srcOffset = srcOffset;
    region.dstOffset = dstOffset;

    vkCmdCopyBuffer(
        commandBuffer->GetVulkanHandle(),
        srcBuffer->m_handle,
        m_handle,
        1,
        &region);
}

RendererResult VulkanGpuBuffer::Create()
{
    if (IsCreated())
    {
        // already created
        return {};
    }

    m_vkBufferUsageFlags = GetVkUsageFlags(m_type);
    m_vmaUsage = GetVmaMemoryUsage(m_type, m_cpuAccessible);
    m_vmaAllocationCreateFlags = GetVkAllocationCreateFlags(m_type, m_cpuAccessible);

    if (m_size == 0)
    {
        Assert("Creating empty gpu buffer will result in errors!");

        return HYP_MAKE_ERROR(RendererError, "Creating empty gpu buffer will result in errors!");
    }

    const auto createInfo = GetBufferCreateInfo();
    const auto allocInfo = GetAllocationCreateInfo();

    CheckResultOrReturn(CheckCanAllocate(createInfo, allocInfo, m_size));

    if (m_alignment != 0)
    {
        VULKAN_CHECK_MSG(
            vmaCreateBufferWithAlignment(
                RI.GetDevice()->GetVmaAllocator(),
                &createInfo,
                &allocInfo,
                m_alignment,
                &m_handle,
                &m_vmaAllocation,
                nullptr),
            "Failed to create aligned gpu buffer!");
    }
    else
    {
        VULKAN_CHECK_MSG(
            vmaCreateBuffer(
                RI.GetDevice()->GetVmaAllocator(),
                &createInfo,
                &allocInfo,
                &m_handle,
                &m_vmaAllocation,
                nullptr),
            "Failed to create gpu buffer!");
    }

    if (IsCpuAccessible())
    {
        Map();

        // Memset all to zero
        Memory::Fill(m_mapping, 0, m_size);
    }

#if HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    return {};
}

RendererResult VulkanGpuBuffer::EnsureCapacity(
    size_t minimumSize,
    size_t alignment,
    bool* outSizeChanged)
{
    if (minimumSize == 0)
    {
        return {};
    }

    if (minimumSize <= m_size)
    {
        if (outSizeChanged != nullptr)
        {
            *outSizeChanged = false;
        }

        return {};
    }

    bool shouldCreate = IsCreated();

    if (shouldCreate)
    {
        if (m_mapping != nullptr)
        {
            Unmap();
        }

        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle, allocation = m_vmaAllocation]() -> void
            {
                vmaDestroyBuffer(RI.GetDevice()->GetVmaAllocator(), handle, allocation);
            }));

        m_handle = VK_NULL_HANDLE;
        m_vmaAllocation = VK_NULL_HANDLE;
        m_resourceState = RS_UNDEFINED;
    }

    m_size = minimumSize;
    m_alignment = alignment;

    if (outSizeChanged != nullptr)
    {
        *outSizeChanged = true;
    }

    if (shouldCreate)
    {
        CheckResultOrReturn(Create());
    }

    return {};
}

RendererResult VulkanGpuBuffer::EnsureCapacity(
    size_t minimumSize,
    bool* outSizeChanged)
{
    return EnsureCapacity(minimumSize, 0, outSizeChanged);
}

VkBufferCreateInfo VulkanGpuBuffer::GetBufferCreateInfo() const
{
    const QueueFamilyIndices& qfIndices = RI.GetDevice()->GetQueueFamilyIndices();
    const uint32 bufferFamilyIndices[] = { qfIndices.graphicsFamily.Get(), qfIndices.computeFamily.Get() };

    VkBufferCreateInfo vkBufferInfo { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vkBufferInfo.size = m_size;
    vkBufferInfo.usage = m_vkBufferUsageFlags;
    vkBufferInfo.pQueueFamilyIndices = bufferFamilyIndices;
    vkBufferInfo.queueFamilyIndexCount = ArraySize(bufferFamilyIndices);

    return vkBufferInfo;
}

VmaAllocationCreateInfo VulkanGpuBuffer::GetAllocationCreateInfo() const
{
    VmaAllocationCreateInfo allocInfo {};
    allocInfo.flags = m_vmaAllocationCreateFlags;
    allocInfo.usage = m_vmaUsage;

    return allocInfo;
}

RendererResult VulkanGpuBuffer::CheckCanAllocate(
    const VkBufferCreateInfo& bufferCreateInfo,
    const VmaAllocationCreateInfo& allocationCreateInfo,
    size_t size) const
{
    const VulkanFeatures& features = RI.GetDevice()->GetFeatures();

    RendererResult result;

    uint32 memoryTypeIndex = UINT32_MAX;

    VULKAN_PASS_ERRORS(
        vmaFindMemoryTypeIndexForBufferInfo(
            RI.GetDevice()->GetVmaAllocator(),
            &bufferCreateInfo,
            &allocationCreateInfo,
            &memoryTypeIndex),
        result);

    /* check that we have enough space in the memory type */
    const auto& memoryProperties = features.GetPhysicalDeviceMemoryProperties();

    Assert(memoryTypeIndex < memoryProperties.memoryTypeCount);

    const auto heapIndex = memoryProperties.memoryTypes[memoryTypeIndex].heapIndex;
    const auto& heap = memoryProperties.memoryHeaps[heapIndex];

    if (heap.size < size)
    {
        return HYP_MAKE_ERROR(RendererError, "Heap size is less than requested size. "
                                             "Maybe the wrong memory type has been requested, or the device is out of memory.");
    }

    return result;
}

#if HYP_DEBUG_MODE

void VulkanGpuBuffer::SetDebugName(Name name)
{
    GpuBufferBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    const char* strName = name.LookupString();

    if (m_vmaAllocation != VK_NULL_HANDLE)
    {
        vmaSetAllocationName(RI.GetDevice()->GetVmaAllocator(), m_vmaAllocation, strName);
    }

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
    objectNameInfo.objectHandle = (uint64)m_handle;
    objectNameInfo.pObjectName = strName;

    RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

#pragma endregion VulkanGpuBuffer

} // namespace Hyperion
