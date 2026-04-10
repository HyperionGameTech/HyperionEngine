#pragma once

#include "RxRenderPass.hpp"
#include "RxShader.hpp"

#include <vulkan/vulkan.h>

#include <Core/FxRef.hpp>
#include <Core/FxSizedArray.hpp>
#include <Core/FxSlice.hpp>
#include <Renderer/RxVertex.hpp>

struct RxVertexDescription;
class RxCommandBuffer;
class RxGpuDevice;

struct alignas(16) FxDrawPushConstants
{
    float32 CameraMatrix[16];
    uint32 ObjectId = 0;
    uint32 MaterialIndex = 0;
};

struct alignas(16) FxLightVertPushConstants
{
    float32 CameraMatrix[16];
    uint32 ObjectId = 0;
};

struct alignas(16) FxCompositionPushConstants
{
    float32 ViewInverse[16];
    float32 ProjInverse[16];
};

struct RxPipelineProperties
{
    VkCullModeFlags CullMode = VK_CULL_MODE_NONE;
    VkFrontFace WindingOrder = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPolygonMode PolygonMode = VK_POLYGON_MODE_FILL;

    bool bDisableDepthTest : 1 = false;
    bool bDisableDepthWrite : 1 = false;

    FxVec2u ViewportSize = FxVec2u::sZero;
    VkCompareOp DepthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
};

struct RxPushConstants
{
    uint32 Size;
    VkShaderStageFlags StageFlags;
};


class RxPipeline
{
public:
    void Create(const std::string& name, const FxSlice<FxRef<RxShaderProgram>>& shaders,
                const FxSlice<VkAttachmentDescription>& attachments,
                const FxSlice<VkPipelineColorBlendAttachmentState>& color_blend_attachments,
                RxVertexDescription* vertex_info, const RxRenderPass& render_pass,
                const RxPipelineProperties& properties);

    FX_FORCE_INLINE void SetLayout(VkPipelineLayout layout) { Layout = layout; }

    static VkPipelineLayout CreateLayout(const FxSlice<const RxPushConstants>& push_constant_defs,
                                         const FxSlice<VkDescriptorSetLayout>& descriptor_set_layouts);

    void Destroy();

    void Bind(const RxCommandBuffer& command_buffer);

    ~RxPipeline() { Destroy(); }

private:
public:
    VkPipelineLayout Layout = nullptr;
    VkPipeline Pipeline = nullptr;

    // XXX: TEMP
    FxRef<RxShaderProgram> VertexShader { nullptr };
    FxRef<RxShaderProgram> FragmentShader { nullptr };


private:
    RxGpuDevice* mDevice = nullptr;

protected:
    friend class RxPipelineBuilder;

    bool mbDoNotDestroyLayout = false;
};
