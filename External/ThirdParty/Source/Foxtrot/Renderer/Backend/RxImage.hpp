#pragma once

#include <vulkan/vulkan.h>

// #define VMA_DEBUG_LOG(...) OldLog::Warning(__VA_ARGS__)

#include "RxCommands.hpp"
#include "RxGpuBuffer.hpp"

#include <ThirdParty/vk_mem_alloc.h>

#include <Core/FxRef.hpp>
#include <Math/FxVec2.hpp>

#include <optional>

enum class RxImageFormat
{
    eNone,

    // Color formats

    eBGRA8_UNorm,
    eRGBA8_SRGB,
    eRGBA8_UNorm,

    eRG32_Float,

    eRGBA16_Float,

    eRGB32_Float,

    /// DO NOT USE FORMAT: Marker for depth formats
    _eDepthFormatsBegin,

    eD16_UNorm_S8_UInt,
    eD32_Float,
    eD32_Float_S8_UInt,

    /// DO NOT USE FORMAT: Marker for depth formats
    _eDepthFormatsEnd,
};


struct RxImageFormatUtil
{
    static constexpr bool IsDepth(RxImageFormat format)
    {
        if (format > RxImageFormat::_eDepthFormatsBegin && format < RxImageFormat::_eDepthFormatsEnd) {
            return true;
        }

        return false;
    }

    /**
     * @brief Get the size of the format in bytes. For example, RGBA8 would return 4.
     */
    static constexpr uint32 GetSize(RxImageFormat format)
    {
        switch (format) {
        case RxImageFormat::eNone:
        case RxImageFormat::_eDepthFormatsBegin:
        case RxImageFormat::_eDepthFormatsEnd:
            break;

            // Color formats

        case RxImageFormat::eBGRA8_UNorm:
        case RxImageFormat::eRGBA8_SRGB:
        case RxImageFormat::eRGBA8_UNorm:
            return 4;

        case RxImageFormat::eRG32_Float:
        case RxImageFormat::eRGBA16_Float:
            return 8;

        case RxImageFormat::eRGB32_Float:
            return 12;

            // Depth formats

        case RxImageFormat::eD16_UNorm_S8_UInt:
            return 3;

        case RxImageFormat::eD32_Float:
            return 4;

        case RxImageFormat::eD32_Float_S8_UInt:
            return 5;
        }

        return 0;
    }


    static constexpr VkFormat ToUnderlying(RxImageFormat format)
    {
        switch (format) {
        case RxImageFormat::eNone:
        case RxImageFormat::_eDepthFormatsBegin:
        case RxImageFormat::_eDepthFormatsEnd:
            break;

            // Color formats

        case RxImageFormat::eRG32_Float:
            return VK_FORMAT_R32G32_SFLOAT;
        case RxImageFormat::eBGRA8_UNorm:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case RxImageFormat::eRGBA8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case RxImageFormat::eRGBA8_UNorm:
            return VK_FORMAT_R8G8B8A8_UNORM;

        case RxImageFormat::eRGBA16_Float:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case RxImageFormat::eRGB32_Float:
            return VK_FORMAT_R32G32B32_SFLOAT;

            // Depth Formats

        case RxImageFormat::eD16_UNorm_S8_UInt:
            return VK_FORMAT_D16_UNORM_S8_UINT;
        case RxImageFormat::eD32_Float:
            return VK_FORMAT_D32_SFLOAT;
        case RxImageFormat::eD32_Float_S8_UInt:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        }

        return VK_FORMAT_UNDEFINED;
    }
};


struct RxImageTypeProperties
{
    VkImageViewType ViewType;
    uint32 LayerCount;
};

enum class RxImageType
{
    e2d,
    eCubemap,
};

enum class RxImageAspectFlag
{
    eColor = VK_IMAGE_ASPECT_COLOR_BIT,
    eDepth = VK_IMAGE_ASPECT_DEPTH_BIT,
};

struct RxTransitionLayoutOverrides
{
    std::optional<VkPipelineStageFlagBits> DstStage = std::nullopt;
    std::optional<VkAccessFlags> DstAccessMask = std::nullopt;
};

struct RxImageCubemapOptions
{
    RxImageAspectFlag AspectFlag = RxImageAspectFlag::eColor;
};

const RxImageTypeProperties RxImageTypeGetProperties(RxImageType image_type);

class RxImage
{
public:
    RxImage();
    RxImage(const RxImage& other);

    /**
     * @brief Transfers an `FxRef` to the normal ref counted image.
     */
    RxImage(FxRef<RxImage>&& ref);

    RxImage& operator=(const RxImage& other);
    RxImage& operator=(FxRef<RxImage>&& ref);

    void Create(RxImageType image_type, const FxVec2u& size, RxImageFormat format, VkImageTiling tiling,
                VkImageUsageFlags usage, RxImageAspectFlag aspect);

    void Create(RxImageType image_type, const FxVec2u& size, RxImageFormat format, VkImageUsageFlags usage,
                RxImageAspectFlag aspect);

    void CreateGpuOnly(RxImageType image_type, const FxVec2u& size, RxImageFormat format,
                       const FxSizedArray<uint8>& image_data);

    void TransitionLayout(VkImageLayout new_layout, RxCommandBuffer& cmd, uint32 layer_count = 1,
                          std::optional<RxTransitionLayoutOverrides> overrides = std::nullopt);

    void TransitionDepthToShaderRO(RxCommandBuffer& cmd);
    void TransitionDepthToAttachment(RxCommandBuffer& cmd);


    void CopyFromBuffer(const RxRawGpuBuffer& buffer, VkImageLayout final_layout, FxVec2u size, uint32 base_layer = 0);

    void CreateLayeredImageFromCubemap(RxImage& cubemap, RxImageFormat image_format, VkImageAspectFlags aspect_flags,
                                       RxImageCubemapOptions options);

    FX_FORCE_INLINE bool IsInited() const { return (Image != nullptr); }

    void DecRef();

    ~RxImage();

public:
    FxVec2u Size = FxVec2u::sZero;

    RxImageAspectFlag Aspect = RxImageAspectFlag::eColor;

    VkImage Image = nullptr;
    VkImageView View = nullptr;

    RxImageType ViewType = RxImageType::e2d;
    RxImageFormat Format = RxImageFormat::eNone;
    VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocation Allocation = nullptr;

    FxRefCount* mpRefCnt = nullptr;
};
