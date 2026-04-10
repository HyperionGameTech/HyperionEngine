#pragma once

#include "Backend/RxPipeline.hpp"
#include "Backend/RxShader.hpp"
#include "RxAttachment.hpp"

#include <Core/FxRef.hpp>
#include <Core/FxSizedArray.hpp>
#include <Core/FxStackArray.hpp>


using RxColorComponentFlags = uint32;
enum RxColorComponent : uint32
{
    RxColorComponent_None = 0x0000,

    RxColorComponent_R = VK_COLOR_COMPONENT_R_BIT,
    RxColorComponent_G = VK_COLOR_COMPONENT_G_BIT,
    RxColorComponent_B = VK_COLOR_COMPONENT_B_BIT,
    RxColorComponent_A = VK_COLOR_COMPONENT_A_BIT,

    RxColorComponent_RG = (RxColorComponent_R | RxColorComponent_G),
    RxColorComponent_RGB = (RxColorComponent_RG | RxColorComponent_B),
    RxColorComponent_RGBA = (RxColorComponent_RGB | RxColorComponent_A),
};


union RxPipelineBlendFactor
{
    struct
    {
        VkBlendFactor Src : 8;
        VkBlendFactor Dst : 8;
    } Ops;

    uint16 Value;
};

union RxPipelineBlendOp
{
    struct
    {
        VkBlendOp Alpha : 8;
        VkBlendOp Color : 8;
    } Ops;

    uint16 Value;
};

struct RxPipelineBlendAttachment
{
    bool Enabled = true;
    RxColorComponentFlags Mask = RxColorComponent_RGBA;
    RxPipelineBlendOp BlendOp = { .Ops = { .Alpha = VK_BLEND_OP_ADD, .Color = VK_BLEND_OP_ADD } };

    RxPipelineBlendFactor AlphaBlend = { .Ops = { .Src = VK_BLEND_FACTOR_ZERO, .Dst = VK_BLEND_FACTOR_ZERO } };

    RxPipelineBlendFactor ColorBlend = { .Ops = { .Src = VK_BLEND_FACTOR_ZERO, .Dst = VK_BLEND_FACTOR_ZERO } };
};


// RxPipelineBuilder::Build() // Build pipeline
//     .AddBlendAttachment({ .Enabled = true, .AlphaBlend = { .Src = VK_BLEND_FACTOR_ONE, .Dst = VK_BLEND_FACTOR_ZERO }
//     }) .AddBlendAttachment(RxColorComponent_RGBA, false) .AddColorAttachment({
//         .Format = VK_FORMAT_B8G8R8A8_UNORM,
//     });


// using RxBlendFactorFlags = uint32;
// enum RxBlendFactor : uint32
// {
//     RxBlendFactorSrc_Zero = (VK_BLEND_FACTOR_ZERO << 8),
//     RxBlendFactorSrc_One = (VK_BLEND_FACTOR_ONE << 8),
//     RxBlendFactorSrc_SrcAlpha = (VK_BLEND_FACTOR_SRC_ALPHA << 8),
//     RxBlendFactorSrc_DstAlpha = (VK_BLEND_FACTOR_DST_ALPHA << 8),

//     RxBlendFactorDst_Zero = (VK_BLEND_FACTOR_ZERO & 0xFF),
//     RxBlendFactorDst_One = (VK_BLEND_FACTOR_ONE & 0xFF),
//     RxBlendFactorDst_SrcAlpha = (VK_BLEND_FACTOR_SRC_ALPHA & 0xFF),
//     RxBlendFactorDst_DstAlpha = (VK_BLEND_FACTOR_DST_ALPHA & 0xFF),
// };


class RxPipelineBuilder
{
    static constexpr uint32 scMinAttachmentCount = 10;

public:
    RxPipelineBuilder() {}

    static RxPipelineBuilder Make() { return {}; }

    FX_FORCE_INLINE RxPipelineBuilder& AddBlendAttachment(const RxPipelineBlendAttachment& attachment)
    {
        if (mBlendAttachments == nullptr) {
            mBlendAttachments.InitCapacity(scMinAttachmentCount);
        }

        mBlendAttachments.Insert(attachment);

        return *this;
    }

    FX_FORCE_INLINE RxPipelineBuilder& SetAttachments(RxTargetList* attachment_list)
    {
        mpAttachmentList = attachment_list;

        return *this;
    }

    FX_FORCE_INLINE RxPipelineBuilder& SetShaders(const FxRef<RxShaderProgram>& vertex,
                                                  const FxRef<RxShaderProgram>& fragment)
    {
        mVertexShader = vertex;
        mFragmentShader = fragment;

        return *this;
    }

    FX_FORCE_INLINE RxPipelineBuilder& SetVertexDescription(RxVertexDescription* vertex_info)
    {
        mVertexInfo = vertex_info;
        return *this;
    }

    FX_FORCE_INLINE RxPipelineBuilder& SetLayout(VkPipelineLayout layout)
    {
        // This is a new pipeline
        mbDidFirstBuild = false;
        mLayout = layout;
        return *this;
    }

    FX_FORCE_INLINE RxPipelineBuilder& SetRenderPass(RxRenderPass* render_pass)
    {
        mRenderPass = render_pass;
        return *this;
    }

    FX_FORCE_INLINE RxPipelineBuilder& SetName(const std::string& name)
    {
        mPipelineName = name;
        return *this;
    }

    /////////////////////////////
    // Properties setters
    /////////////////////////////


    FX_FORCE_INLINE RxPipelineBuilder& SetProperties(const RxPipelineProperties& properties)
    {
        mProperties = properties;
        return *this;
    }

    FX_FORCE_INLINE RxPipelineBuilder& SetWindingOrder(VkFrontFace winding_order)
    {
        mProperties.WindingOrder = winding_order;
        return *this;
    }

    FX_FORCE_INLINE RxPipelineBuilder& SetCullMode(VkCullModeFlags cull_mode)
    {
        mProperties.CullMode = cull_mode;
        return *this;
    }

    FX_FORCE_INLINE RxPipelineBuilder& SetPolygonMode(VkPolygonMode polygon_mode)
    {
        mProperties.PolygonMode = polygon_mode;
        return *this;
    }


    void Build(RxPipeline& pipeline)
    {
        FxSizedArray<VkPipelineColorBlendAttachmentState> vk_blend_attachments;

        // Make vulkan blend attachments
        {
            vk_blend_attachments.InitCapacity(mBlendAttachments.Size);

            for (const RxPipelineBlendAttachment& am : mBlendAttachments) {
                vk_blend_attachments.Insert(VkPipelineColorBlendAttachmentState {
                    .blendEnable = am.Enabled ? 1U : 0U,
                    .srcColorBlendFactor = am.ColorBlend.Ops.Src,
                    .dstColorBlendFactor = am.ColorBlend.Ops.Dst,
                    .colorBlendOp = am.BlendOp.Ops.Color,
                    .srcAlphaBlendFactor = am.AlphaBlend.Ops.Src,
                    .dstAlphaBlendFactor = am.AlphaBlend.Ops.Dst,
                    .alphaBlendOp = am.BlendOp.Ops.Alpha,
                    .colorWriteMask = am.Mask,
                });
            }
        }

        FxSizedArray<FxRef<RxShaderProgram>> shader_list = { mVertexShader, mFragmentShader };

        // Make vulkan blend attachments

        pipeline.Layout = mLayout;

        FxLogInfo("Creating pipeline '{}'", mPipelineName);
        pipeline.Create(mPipelineName, shader_list, mpAttachmentList->GetDescriptions(), vk_blend_attachments,
                        mVertexInfo, *mRenderPass, mProperties);

        if (mbDidFirstBuild) {
            pipeline.mbDoNotDestroyLayout = true;
        }

        mbDidFirstBuild = true;
    }

private:
    FxSizedArray<RxPipelineBlendAttachment> mBlendAttachments;
    RxTargetList* mpAttachmentList = nullptr;

    VkPipelineLayout mLayout = nullptr;

    std::string mPipelineName = "Unnamed Pipeline";

    FxRef<RxShaderProgram> mVertexShader { nullptr };
    FxRef<RxShaderProgram> mFragmentShader { nullptr };

    RxRenderPass* mRenderPass { nullptr };

    RxVertexDescription* mVertexInfo { nullptr };
    RxPipelineProperties mProperties;

    bool mbDidFirstBuild = false;
};
