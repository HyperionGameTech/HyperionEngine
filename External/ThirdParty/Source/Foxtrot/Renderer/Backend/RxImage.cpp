
#include "RxImage.hpp"

#include "../RxRenderBackend.hpp"

#include <Core/FxDefines.hpp>
#include <Core/FxPanic.hpp>
#include <Core/FxStackArray.hpp>
#include <Core/MemPool/FxMemPool.hpp>
#include <FxEngine.hpp>
#include <Renderer/Backend/RxUtil.hpp>
#include <Renderer/RxGlobals.hpp>

FX_SET_MODULE_NAME("RxImage")

const RxImageTypeProperties RxImageTypeGetProperties(RxImageType image_type)
{
    RxImageTypeProperties props {};

    if (image_type == RxImageType::e2d) {
        props.ViewType = VK_IMAGE_VIEW_TYPE_2D;
        props.LayerCount = 1;
    }
    else if (image_type == RxImageType::eCubemap) {
        props.ViewType = VK_IMAGE_VIEW_TYPE_CUBE;
        props.LayerCount = 6;
    }
    else {
        FxLogError("Unknown image type!");
    }

    return props;
}

RxImage::RxImage() { mpRefCnt = gEnginePool->Alloc<FxRefCount>(sizeof(FxRefCount)); }

RxImage::RxImage(const RxImage& other) { (*this) = other; }

RxImage::RxImage(FxRef<RxImage>&& ref) { (*this) = std::move(ref); }

RxImage& RxImage::operator=(FxRef<RxImage>&& ref)
{
    // Set the image properties, move the ref count over.
    (*this) = *ref;

    mpRefCnt = ref.mpRefCnt;
    ref.mpRefCnt = nullptr;

    ref.DestroyRef();

    return *this;
}


RxImage& RxImage::operator=(const RxImage& other)
{
    // If this image is already established, then we should decrement the ref here.
    if (mpRefCnt) {
        DecRef();
    }

    Size = other.Size;
    Aspect = other.Aspect;

    Image = other.Image;
    View = other.View;

    ViewType = other.ViewType;
    Format = other.Format;
    ImageLayout = other.ImageLayout;
    Allocation = other.Allocation;

    // Set the new ref count
    mpRefCnt = other.mpRefCnt;

    if (!mpRefCnt) {
        mpRefCnt = gEnginePool->Alloc<FxRefCount>(sizeof(FxRefCount));
    }
    else {
        mpRefCnt->Inc();
    }

    return *this;
}

void RxImage::Create(RxImageType image_type, const FxVec2u& size, RxImageFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, RxImageAspectFlag aspect)
{
    FxAssert(size.X > 0 && size.Y > 0);
    FxAssert(Image == nullptr && Allocation == nullptr);

    Aspect = aspect;
    Size = size;
    Format = format;
    ViewType = image_type;

    if (!mpRefCnt) {
        mpRefCnt = gEnginePool->Alloc<FxRefCount>(sizeof(FxRefCount));
    }

    // Get the vulkan values for the image type
    RxImageTypeProperties image_type_props = RxImageTypeGetProperties(image_type);

    VkImageCreateFlags image_create_flags = 0;

    if (image_type == RxImageType::eCubemap) {
        image_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    // Create the vulkan image
    VkImageCreateInfo image_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = image_create_flags,

        .imageType = VK_IMAGE_TYPE_2D,

        .format = RxImageFormatUtil::ToUnderlying(format),

        .extent = { .width = size.Width(), .height = size.Height(), .depth = 1 },

        .mipLevels = 1,
        .arrayLayers = image_type_props.LayerCount,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,

        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,

        .initialLayout = ImageLayout,

    };

    VmaAllocationCreateInfo create_info {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .priority = 1.0f,
    };

    VkResult status = vmaCreateImage(gRenderer->GpuAllocator, &image_info, &create_info, &Image, &Allocation, nullptr);
    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Could not create vulkan image", status);
    }


    // FxLogInfo("Create Image (Image={:p}, Allocation={:p})", reinterpret_cast<void*>(Image),
    //           reinterpret_cast<void*>(Allocation));

#ifdef FX_DEBUG_GPU_BUFFER_ALLOCATION_NAMES
    {
        static uint32 allocation_number = 0;
        allocation_number += 1;

        std::string allocation_name = "";
        // typeid(ElementType).name();

        char name_buffer[256];

        snprintf(name_buffer, 128, "Img(%u,%u){%u}", size.Width(), size.Height(), allocation_number);

        vmaSetAllocationName(Fx_Fwd_GetGpuAllocator(), Allocation, name_buffer);
    }
#endif

    const VkImageViewCreateInfo view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = Image,
        .viewType = image_type_props.ViewType,
        .format = RxImageFormatUtil::ToUnderlying(format),
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = static_cast<VkImageAspectFlags>(aspect),
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = image_type_props.LayerCount,
            },
    };

    status = vkCreateImageView(gRenderer->GetDevice()->Device, &view_create_info, nullptr, &View);
    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Could not create swapchain image view", status);
    }

    if (reinterpret_cast<uintptr_t>(Image) == 0x2b000000002b) {
        FX_BREAKPOINT;
    }

    std::string image_view_name = "";
    // typeid(ElementType).name();

#ifdef FX_DEBUG_IMAGE_VIEWS
    {
        static int view_allocation_number = 0;
        view_allocation_number++;

        char name_buffer[256];

        snprintf(name_buffer, 128, "View(%u,%u){%u}", size.Width(), size.Height(), view_allocation_number);

        RxUtil::SetDebugLabel(name_buffer, VK_OBJECT_TYPE_IMAGE_VIEW, View);
    }
#endif
}

void RxImage::Create(RxImageType image_type, const FxVec2u& size, RxImageFormat format, VkImageUsageFlags usage,
                     RxImageAspectFlag aspect)
{
    Create(image_type, size, format, VK_IMAGE_TILING_OPTIMAL, usage, aspect);
}


void RxImage::CreateGpuOnly(RxImageType image_type, const FxVec2u& size, RxImageFormat format,
                            const FxSizedArray<uint8>& image_data)
{
    RxRawGpuBuffer staging_buffer;
    staging_buffer.Create(RxGpuBufferType::eTransfer, image_data.Size, VMA_MEMORY_USAGE_CPU_TO_GPU,
                          RxGpuBufferFlags::eTransferReceiver);
    staging_buffer.Upload(image_data);

    const VkImageUsageFlags usage_flags = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    Create(image_type, size, format, VK_IMAGE_TILING_OPTIMAL, usage_flags, RxImageAspectFlag::eColor);

    CopyFromBuffer(staging_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, size);
}

void RxImage::TransitionDepthToShaderRO(RxCommandBuffer& cmd)
{
    VkImageMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,

        .srcAccessMask = static_cast<VkAccessFlags>(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
        .dstAccessMask = static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT),

        .oldLayout = ImageLayout,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

        .image = Image,

        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);

    ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void RxImage::TransitionDepthToAttachment(RxCommandBuffer& cmd)
{
    VkImageMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,

        .srcAccessMask = static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT),
        .dstAccessMask = static_cast<VkAccessFlags>(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),

        .oldLayout = ImageLayout,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,

        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

        .image = Image,

        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void RxImage::TransitionLayout(VkImageLayout new_layout, RxCommandBuffer& cmd, uint32 layer_count,
                               std::optional<RxTransitionLayoutOverrides> overrides)
{
    bool is_depth_texture = RxImageFormatUtil::IsDepth(Format);

    VkImageAspectFlags aspect_flags = static_cast<VkImageAspectFlags>((is_depth_texture) ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                                         : VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,

        .srcAccessMask = static_cast<VkAccessFlags>((is_depth_texture) ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
        .dstAccessMask = static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT),

        .oldLayout = ImageLayout,
        .newLayout = new_layout,

        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

        .image = Image,

        .subresourceRange =
            {
                .aspectMask = aspect_flags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = layer_count,
            },
    };

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkPipelineStageFlags dest_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    if (ImageLayout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (ImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    if (is_depth_texture) {
        src_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    if (overrides.has_value()) {
        if (overrides->DstStage.has_value()) {
            dest_stage = overrides->DstStage.value();
        }
        if (overrides->DstAccessMask.has_value()) {
            barrier.dstAccessMask = overrides->DstAccessMask.value();
        }
    }

    vkCmdPipelineBarrier(cmd, src_stage, dest_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    ImageLayout = new_layout;
}


void RxImage::CopyFromBuffer(const RxRawGpuBuffer& buffer, VkImageLayout final_layout, FxVec2u size, uint32 base_layer)
{
    Fx_Fwd_SubmitUploadCmd(
        [&](RxCommandBuffer& cmd)
        {
            TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd, 1,
                             RxTransitionLayoutOverrides { .DstStage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                           .DstAccessMask = VK_ACCESS_TRANSFER_READ_BIT });

            VkBufferImageCopy copy {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = base_layer,
                    .layerCount = 1,
                },
                .imageExtent =
                    VkExtent3D {
                        .width = size.X,
                        .height = size.Y,
                        .depth = 1,
                    },
            };

            vkCmdCopyBufferToImage(cmd, buffer.Buffer, Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

            TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd, 1,
                             RxTransitionLayoutOverrides { .DstStage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                           .DstAccessMask = VK_ACCESS_TRANSFER_READ_BIT });
        });
}

enum CubemapLayer
{
    // Forward,

    Right,
    Left,
    Top,
    Bottom,
    Front,
    Back,
};

void RxImage::CreateLayeredImageFromCubemap(RxImage& cubemap, RxImageFormat image_format,
                                            VkImageAspectFlags aspect_flags, RxImageCubemapOptions options)
{
    // Here is the type of cubemap we will be reading here:
    //
    // T -> Top, B -> Bottom, L -> Left, R -> Right
    // FW -> Forward, BW -> Backward
    //
    // +-----+-----+-----+-----+
    // |     |  T  |     |     |
    // +-----+-----+-----+-----+
    // |  L  |  FW |  R  |  BW |
    // +-----+-----+-----+-----+
    // |     |  B  |     |     |
    // +-----+-----+-----+-----+
    //
    // Note that it is 4 tiles wide and 3 tiles tall.


    const uint32 tile_width = cubemap.Size.X / 4;
    const uint32 tile_height = cubemap.Size.Y / 3;

    FxAssert(tile_width == tile_height);


    Create(RxImageType::eCubemap, FxVec2u(tile_width, tile_height), image_format, VK_IMAGE_TILING_OPTIMAL,
           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, Aspect);

    FxStackArray<VkImageCopy, 6> copy_infos;

    VkImageCopy copy_info {
        .srcSubresource = { .aspectMask = aspect_flags, .baseArrayLayer = 0, .layerCount = 1, },
        .dstSubresource = { .aspectMask = aspect_flags, .baseArrayLayer = 0, .layerCount = 1, },
        .dstOffset = { .x = 0, .y = 0 },
        .extent = { .width = tile_width, .height = tile_height, .depth = 1 },
    };

    // Top
    {
        copy_info.srcOffset = { .x = static_cast<int32>(tile_width), .y = 0 };
        copy_info.dstSubresource.baseArrayLayer = CubemapLayer::Top;

        copy_infos.Insert(copy_info);
    }


    // Left
    {
        copy_info.srcOffset = { .x = 0, .y = static_cast<int32>(tile_height) };
        copy_info.dstSubresource.baseArrayLayer = CubemapLayer::Left;


        copy_infos.Insert(copy_info);
    }

    // Front

    {
        copy_info.srcOffset = { .x = static_cast<int32>(tile_width), .y = static_cast<int32>(tile_height) };
        copy_info.dstSubresource.baseArrayLayer = CubemapLayer::Front;


        copy_infos.Insert(copy_info);
    }

    // Forward
    {
        copy_info.srcOffset = { .x = static_cast<int32>(tile_width) * 2, .y = static_cast<int32>(tile_height) };
        copy_info.dstSubresource.baseArrayLayer = CubemapLayer::Right;

        copy_infos.Insert(copy_info);
    }

    // Back
    {
        copy_info.srcOffset = { .x = static_cast<int32>(tile_width) * 3, .y = static_cast<int32>(tile_height) };
        copy_info.dstSubresource.baseArrayLayer = CubemapLayer::Back;

        copy_infos.Insert(copy_info);
    }

    // Bottom
    {
        copy_info.srcOffset = { .x = static_cast<int32>(tile_width), .y = static_cast<int32>(tile_height) * 2 };
        copy_info.dstSubresource.baseArrayLayer = CubemapLayer::Bottom;

        copy_infos.Insert(copy_info);
    }


    gRenderer->SubmitOneTimeCmd(
        [&](RxCommandBuffer& cmd)
        {
            cubemap.TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmd);
            TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd, 6);

            vkCmdCopyImage(cmd.CommandBuffer, cubemap.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, copy_infos.pData);

            TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd, 6);
        });
}

void RxImage::DecRef()
{
    if (!mpRefCnt) {
        return;
    }

    if (mpRefCnt->Dec() > 0) {
        return;
    }

    gEnginePool->Free(mpRefCnt);
    mpRefCnt = nullptr;

    if (View != nullptr) {
        vkDestroyImageView(gRenderer->GetDevice()->Device, View, nullptr);
    }

    if (Image != nullptr && Allocation != nullptr) {
        vmaDestroyImage(gRenderer->GpuAllocator, this->Image, this->Allocation);
    }

    Image = nullptr;
    Allocation = nullptr;
    View = nullptr;
}


RxImage::~RxImage() { DecRef(); }
