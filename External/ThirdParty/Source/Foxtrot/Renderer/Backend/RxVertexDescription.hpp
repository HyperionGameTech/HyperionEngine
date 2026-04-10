#pragma once

#include <vulkan/vulkan.h>

#include <Core/FxSizedArray.hpp>
#include <Renderer/RxVertex.hpp>

struct RxVertexDescription
{
    VkVertexInputBindingDescription Binding;
    FxSizedArray<VkVertexInputAttributeDescription> Attributes;

    bool bIsInited : 1 = false;
};

namespace RxVertexUtil {


template <RxVertexType TVertexType>
RxVertexDescription BuildDescription()
{
    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(RxVertex<TVertexType>),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    FxSizedArray<VkVertexInputAttributeDescription> attribs;

    if constexpr (TVertexType == RxVertexType::eSlim) {
        attribs = {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            },
        };
    }
    else if constexpr (TVertexType == RxVertexType::eDefault) {
        using VertexType = RxVertex<RxVertexType::eDefault>;
        attribs = {
            // Position
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            },
            // Normal
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(VertexType, Normal),
            },
            // UV
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(VertexType, UV),
            },
            // Tangent
            {
                .location = 3,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(VertexType, Tangent),
            },
        };
    }

    else if constexpr (TVertexType == RxVertexType::eSkinned) {
        using VertexType = RxVertex<RxVertexType::eSkinned>;
        attribs = {
            // Position
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            },
            // Normal
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(VertexType, Normal),
            },
            // UV
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(VertexType, UV),
            },
            // Tangent
            {
                .location = 3,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(VertexType, Tangent),
            },
            // Bone IDs
            {
                .location = 4,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_UINT,
                .offset = offsetof(VertexType, BoneIds),
            },
            // Bone Weights
            {
                .location = 5,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(VertexType, BoneWeights),
            },
        };
    }
    else {
        FxLogError("Unsupported vertex type!");
    }

    return { binding_desc, std::move(attribs), true };
}

FX_FORCE_INLINE RxVertexDescription BuildDescription(RxVertexType vertex_type)
{
    switch (vertex_type) {
    case RxVertexType::eSlim:
        return RxVertexUtil::BuildDescription<RxVertexType::eSlim>();
    case RxVertexType::eDefault:
        return RxVertexUtil::BuildDescription<RxVertexType::eDefault>();
    case RxVertexType::eSkinned:
        return RxVertexUtil::BuildDescription<RxVertexType::eSkinned>();
    default:;
    }

    FxLogError("Unsupported vertex type!");

    return RxVertexDescription {};
}
}; // namespace RxVertexUtil
