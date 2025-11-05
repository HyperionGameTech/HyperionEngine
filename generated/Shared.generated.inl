#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region BlendFunction Reflection Data

HYP_BEGIN_STRUCT(BlendFunction, 296, 0, {}, ClassAttribute("serialize", "bitwise"),ClassAttribute("size", 4))
HYP_END_STRUCT

#pragma endregion BlendFunction Reflection Data

static_assert(sizeof(BlendFunction) == 4, "Expected sizeof(BlendFunction) to be 4 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region Topology Reflection Data

HYP_BEGIN_ENUM(Topology, 297, 0, {})
    StaticField(NAME(HYP_STR(TOP_TRIANGLES)), Topology::TOP_TRIANGLES),
    StaticField(NAME(HYP_STR(TOP_TRIANGLE_FAN)), Topology::TOP_TRIANGLE_FAN),
    StaticField(NAME(HYP_STR(TOP_TRIANGLE_STRIP)), Topology::TOP_TRIANGLE_STRIP),
    StaticField(NAME(HYP_STR(TOP_LINES)), Topology::TOP_LINES),
    StaticField(NAME(HYP_STR(TOP_POINTS)), Topology::TOP_POINTS)
HYP_END_ENUM

#pragma endregion Topology Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region BlendModeFactor Reflection Data

HYP_BEGIN_ENUM(BlendModeFactor, 298, 0, {})
    StaticField(NAME(HYP_STR(BMF_NONE)), BlendModeFactor::BMF_NONE),
    StaticField(NAME(HYP_STR(BMF_ONE)), BlendModeFactor::BMF_ONE),
    StaticField(NAME(HYP_STR(BMF_ZERO)), BlendModeFactor::BMF_ZERO),
    StaticField(NAME(HYP_STR(BMF_SRC_COLOR)), BlendModeFactor::BMF_SRC_COLOR),
    StaticField(NAME(HYP_STR(BMF_SRC_ALPHA)), BlendModeFactor::BMF_SRC_ALPHA),
    StaticField(NAME(HYP_STR(BMF_DST_COLOR)), BlendModeFactor::BMF_DST_COLOR),
    StaticField(NAME(HYP_STR(BMF_DST_ALPHA)), BlendModeFactor::BMF_DST_ALPHA),
    StaticField(NAME(HYP_STR(BMF_ONE_MINUS_SRC_COLOR)), BlendModeFactor::BMF_ONE_MINUS_SRC_COLOR),
    StaticField(NAME(HYP_STR(BMF_ONE_MINUS_SRC_ALPHA)), BlendModeFactor::BMF_ONE_MINUS_SRC_ALPHA),
    StaticField(NAME(HYP_STR(BMF_ONE_MINUS_DST_COLOR)), BlendModeFactor::BMF_ONE_MINUS_DST_COLOR),
    StaticField(NAME(HYP_STR(BMF_ONE_MINUS_DST_ALPHA)), BlendModeFactor::BMF_ONE_MINUS_DST_ALPHA),
    StaticField(NAME(HYP_STR(BMF_MAX)), BlendModeFactor::BMF_MAX)
HYP_END_ENUM

#pragma endregion BlendModeFactor Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GpuElemType Reflection Data

HYP_BEGIN_ENUM(GpuElemType, 299, 0, {})
    StaticField(NAME(HYP_STR(GET_UNSIGNED_BYTE)), GpuElemType::GET_UNSIGNED_BYTE),
    StaticField(NAME(HYP_STR(GET_SIGNED_BYTE)), GpuElemType::GET_SIGNED_BYTE),
    StaticField(NAME(HYP_STR(GET_UNSIGNED_SHORT)), GpuElemType::GET_UNSIGNED_SHORT),
    StaticField(NAME(HYP_STR(GET_SIGNED_SHORT)), GpuElemType::GET_SIGNED_SHORT),
    StaticField(NAME(HYP_STR(GET_UNSIGNED_INT)), GpuElemType::GET_UNSIGNED_INT),
    StaticField(NAME(HYP_STR(GET_SIGNED_INT)), GpuElemType::GET_SIGNED_INT),
    StaticField(NAME(HYP_STR(GET_FLOAT)), GpuElemType::GET_FLOAT),
    StaticField(NAME(HYP_STR(GET_MAX)), GpuElemType::GET_MAX)
HYP_END_ENUM

#pragma endregion GpuElemType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region StencilOp Reflection Data

HYP_BEGIN_ENUM(StencilOp, 300, 0, {})
    StaticField(NAME(HYP_STR(SO_KEEP)), StencilOp::SO_KEEP),
    StaticField(NAME(HYP_STR(SO_ZERO)), StencilOp::SO_ZERO),
    StaticField(NAME(HYP_STR(SO_REPLACE)), StencilOp::SO_REPLACE),
    StaticField(NAME(HYP_STR(SO_INCREMENT)), StencilOp::SO_INCREMENT),
    StaticField(NAME(HYP_STR(SO_DECREMENT)), StencilOp::SO_DECREMENT)
HYP_END_ENUM

#pragma endregion StencilOp Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetElementType Reflection Data

HYP_BEGIN_ENUM(DescriptorSetElementType, 301, 0, {})
    StaticField(NAME(HYP_STR(UNSET)), DescriptorSetElementType::UNSET),
    StaticField(NAME(HYP_STR(UNIFORM_BUFFER)), DescriptorSetElementType::UNIFORM_BUFFER),
    StaticField(NAME(HYP_STR(UNIFORM_BUFFER_DYNAMIC)), DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC),
    StaticField(NAME(HYP_STR(SSBO)), DescriptorSetElementType::SSBO),
    StaticField(NAME(HYP_STR(STORAGE_BUFFER_DYNAMIC)), DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC),
    StaticField(NAME(HYP_STR(IMAGE)), DescriptorSetElementType::IMAGE),
    StaticField(NAME(HYP_STR(IMAGE_STORAGE)), DescriptorSetElementType::IMAGE_STORAGE),
    StaticField(NAME(HYP_STR(SAMPLER)), DescriptorSetElementType::SAMPLER),
    StaticField(NAME(HYP_STR(TLAS)), DescriptorSetElementType::TLAS),
    StaticField(NAME(HYP_STR(MAX)), DescriptorSetElementType::MAX)
HYP_END_ENUM

#pragma endregion DescriptorSetElementType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureWrapMode Reflection Data

HYP_BEGIN_ENUM(TextureWrapMode, 302, 0, {})
    StaticField(NAME(HYP_STR(TWM_CLAMP_TO_EDGE)), TextureWrapMode::TWM_CLAMP_TO_EDGE),
    StaticField(NAME(HYP_STR(TWM_CLAMP_TO_BORDER)), TextureWrapMode::TWM_CLAMP_TO_BORDER),
    StaticField(NAME(HYP_STR(TWM_REPEAT)), TextureWrapMode::TWM_REPEAT)
HYP_END_ENUM

#pragma endregion TextureWrapMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region StencilCompareOp Reflection Data

HYP_BEGIN_ENUM(StencilCompareOp, 303, 0, {})
    StaticField(NAME(HYP_STR(SCO_ALWAYS)), StencilCompareOp::SCO_ALWAYS),
    StaticField(NAME(HYP_STR(SCO_NEVER)), StencilCompareOp::SCO_NEVER),
    StaticField(NAME(HYP_STR(SCO_EQUAL)), StencilCompareOp::SCO_EQUAL),
    StaticField(NAME(HYP_STR(SCO_NOT_EQUAL)), StencilCompareOp::SCO_NOT_EQUAL)
HYP_END_ENUM

#pragma endregion StencilCompareOp Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ImageSupport Reflection Data

HYP_BEGIN_ENUM(ImageSupport, 304, 0, {})
    StaticField(NAME(HYP_STR(IS_SRV)), ImageSupport::IS_SRV),
    StaticField(NAME(HYP_STR(IS_UAV)), ImageSupport::IS_UAV),
    StaticField(NAME(HYP_STR(IS_DEPTH)), ImageSupport::IS_DEPTH)
HYP_END_ENUM

#pragma endregion ImageSupport Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region FillMode Reflection Data

HYP_BEGIN_ENUM(FillMode, 305, 0, {})
    StaticField(NAME(HYP_STR(FM_FILL)), FillMode::FM_FILL),
    StaticField(NAME(HYP_STR(FM_LINE)), FillMode::FM_LINE)
HYP_END_ENUM

#pragma endregion FillMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DefaultImageFormat Reflection Data

HYP_BEGIN_ENUM(DefaultImageFormat, 306, 0, {})
    StaticField(NAME(HYP_STR(DIF_NONE)), DefaultImageFormat::DIF_NONE),
    StaticField(NAME(HYP_STR(DIF_COLOR)), DefaultImageFormat::DIF_COLOR),
    StaticField(NAME(HYP_STR(DIF_DEPTH)), DefaultImageFormat::DIF_DEPTH),
    StaticField(NAME(HYP_STR(DIF_NORMALS)), DefaultImageFormat::DIF_NORMALS),
    StaticField(NAME(HYP_STR(DIF_STORAGE)), DefaultImageFormat::DIF_STORAGE)
HYP_END_ENUM

#pragma endregion DefaultImageFormat Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureType Reflection Data

HYP_BEGIN_ENUM(TextureType, 307, 0, {})
    StaticField(NAME(HYP_STR(TT_INVALID)), TextureType::TT_INVALID),
    StaticField(NAME(HYP_STR(TT_TEX2_D)), TextureType::TT_TEX2D),
    StaticField(NAME(HYP_STR(TT_TEX3_D)), TextureType::TT_TEX3D),
    StaticField(NAME(HYP_STR(TT_CUBEMAP)), TextureType::TT_CUBEMAP),
    StaticField(NAME(HYP_STR(TT_TEX2_D_ARRAY)), TextureType::TT_TEX2D_ARRAY),
    StaticField(NAME(HYP_STR(TT_CUBEMAP_ARRAY)), TextureType::TT_CUBEMAP_ARRAY),
    StaticField(NAME(HYP_STR(TT_MAX)), TextureType::TT_MAX)
HYP_END_ENUM

#pragma endregion TextureType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureData Reflection Data

HYP_BEGIN_STRUCT(TextureData, 308, 0, {})
    Field(NAME(HYP_STR(ImageData)), &TextureData::imageData, offsetof(TextureData, imageData), Span<const ClassAttribute> { {ClassAttribute("property", "ImageData"), ClassAttribute("serialize", true), ClassAttribute("compressed", true) } })
HYP_END_STRUCT

#pragma endregion TextureData Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureBaseFormat Reflection Data

HYP_BEGIN_ENUM(TextureBaseFormat, 309, 0, {})
    StaticField(NAME(HYP_STR(TFB_NONE)), TextureBaseFormat::TFB_NONE),
    StaticField(NAME(HYP_STR(TFB_R)), TextureBaseFormat::TFB_R),
    StaticField(NAME(HYP_STR(TFB_RG)), TextureBaseFormat::TFB_RG),
    StaticField(NAME(HYP_STR(TFB_RGB)), TextureBaseFormat::TFB_RGB),
    StaticField(NAME(HYP_STR(TFB_RGBA)), TextureBaseFormat::TFB_RGBA),
    StaticField(NAME(HYP_STR(TFB_BGR)), TextureBaseFormat::TFB_BGR),
    StaticField(NAME(HYP_STR(TFB_BGRA)), TextureBaseFormat::TFB_BGRA),
    StaticField(NAME(HYP_STR(TFB_DEPTH)), TextureBaseFormat::TFB_DEPTH)
HYP_END_ENUM

#pragma endregion TextureBaseFormat Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorTableOffsetMap Reflection Data

HYP_BEGIN_STRUCT(DescriptorTableOffsetMap, 310, 0, {})
HYP_END_STRUCT

#pragma endregion DescriptorTableOffsetMap Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region StoreOperation Reflection Data

HYP_BEGIN_ENUM(StoreOperation, 311, 0, {})
    StaticField(NAME(HYP_STR(UNDEFINED)), StoreOperation::UNDEFINED),
    StaticField(NAME(HYP_STR(NONE)), StoreOperation::NONE),
    StaticField(NAME(HYP_STR(STORE)), StoreOperation::STORE)
HYP_END_ENUM

#pragma endregion StoreOperation Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetOffsetMap Reflection Data

HYP_BEGIN_STRUCT(DescriptorSetOffsetMap, 312, 0, {})
HYP_END_STRUCT

#pragma endregion DescriptorSetOffsetMap Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ImageUsage Reflection Data

HYP_BEGIN_ENUM(ImageUsage, 313, 0, {})
    StaticField(NAME(HYP_STR(IU_NONE)), ImageUsage::IU_NONE),
    StaticField(NAME(HYP_STR(IU_SAMPLED)), ImageUsage::IU_SAMPLED),
    StaticField(NAME(HYP_STR(IU_STORAGE)), ImageUsage::IU_STORAGE),
    StaticField(NAME(HYP_STR(IU_ATTACHMENT)), ImageUsage::IU_ATTACHMENT),
    StaticField(NAME(HYP_STR(IU_BLENDED)), ImageUsage::IU_BLENDED)
HYP_END_ENUM

#pragma endregion ImageUsage Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ResourceState Reflection Data

HYP_BEGIN_ENUM(ResourceState, 314, 0, {})
    StaticField(NAME(HYP_STR(RS_UNDEFINED)), ResourceState::RS_UNDEFINED),
    StaticField(NAME(HYP_STR(RS_PRE_INITIALIZED)), ResourceState::RS_PRE_INITIALIZED),
    StaticField(NAME(HYP_STR(RS_COMMON)), ResourceState::RS_COMMON),
    StaticField(NAME(HYP_STR(RS_VERTEX_BUFFER)), ResourceState::RS_VERTEX_BUFFER),
    StaticField(NAME(HYP_STR(RS_CONSTANT_BUFFER)), ResourceState::RS_CONSTANT_BUFFER),
    StaticField(NAME(HYP_STR(RS_INDEX_BUFFER)), ResourceState::RS_INDEX_BUFFER),
    StaticField(NAME(HYP_STR(RS_RENDER_TARGET)), ResourceState::RS_RENDER_TARGET),
    StaticField(NAME(HYP_STR(RS_UNORDERED_ACCESS)), ResourceState::RS_UNORDERED_ACCESS),
    StaticField(NAME(HYP_STR(RS_DEPTH_STENCIL)), ResourceState::RS_DEPTH_STENCIL),
    StaticField(NAME(HYP_STR(RS_SHADER_RESOURCE)), ResourceState::RS_SHADER_RESOURCE),
    StaticField(NAME(HYP_STR(RS_STREAM_OUT)), ResourceState::RS_STREAM_OUT),
    StaticField(NAME(HYP_STR(RS_INDIRECT_ARG)), ResourceState::RS_INDIRECT_ARG),
    StaticField(NAME(HYP_STR(RS_COPY_DST)), ResourceState::RS_COPY_DST),
    StaticField(NAME(HYP_STR(RS_COPY_SRC)), ResourceState::RS_COPY_SRC),
    StaticField(NAME(HYP_STR(RS_RESOLVE_DST)), ResourceState::RS_RESOLVE_DST),
    StaticField(NAME(HYP_STR(RS_RESOLVE_SRC)), ResourceState::RS_RESOLVE_SRC),
    StaticField(NAME(HYP_STR(RS_PRESENT)), ResourceState::RS_PRESENT),
    StaticField(NAME(HYP_STR(RS_READ_GENERIC)), ResourceState::RS_READ_GENERIC),
    StaticField(NAME(HYP_STR(RS_PREDICATION)), ResourceState::RS_PREDICATION)
HYP_END_ENUM

#pragma endregion ResourceState Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureFormat Reflection Data

HYP_BEGIN_ENUM(TextureFormat, 315, 0, {})
    StaticField(NAME(HYP_STR(TF_NONE)), TextureFormat::TF_NONE),
    StaticField(NAME(HYP_STR(TF_R8)), TextureFormat::TF_R8),
    StaticField(NAME(HYP_STR(TF_RG8)), TextureFormat::TF_RG8),
    StaticField(NAME(HYP_STR(TF_RGB8)), TextureFormat::TF_RGB8),
    StaticField(NAME(HYP_STR(TF_RGBA8)), TextureFormat::TF_RGBA8),
    StaticField(NAME(HYP_STR(TF_B8)), TextureFormat::TF_B8),
    StaticField(NAME(HYP_STR(TF_BG8)), TextureFormat::TF_BG8),
    StaticField(NAME(HYP_STR(TF_BGR8)), TextureFormat::TF_BGR8),
    StaticField(NAME(HYP_STR(TF_BGRA8)), TextureFormat::TF_BGRA8),
    StaticField(NAME(HYP_STR(TF_R16)), TextureFormat::TF_R16),
    StaticField(NAME(HYP_STR(TF_RG16)), TextureFormat::TF_RG16),
    StaticField(NAME(HYP_STR(TF_RGB16)), TextureFormat::TF_RGB16),
    StaticField(NAME(HYP_STR(TF_RGBA16)), TextureFormat::TF_RGBA16),
    StaticField(NAME(HYP_STR(TF_R32)), TextureFormat::TF_R32),
    StaticField(NAME(HYP_STR(TF_RG32)), TextureFormat::TF_RG32),
    StaticField(NAME(HYP_STR(TF_RGB32)), TextureFormat::TF_RGB32),
    StaticField(NAME(HYP_STR(TF_RGBA32)), TextureFormat::TF_RGBA32),
    StaticField(NAME(HYP_STR(TF_R32_)), TextureFormat::TF_R32_),
    StaticField(NAME(HYP_STR(TF_RG16_)), TextureFormat::TF_RG16_),
    StaticField(NAME(HYP_STR(TF_R11_G11_B10_F)), TextureFormat::TF_R11G11B10F),
    StaticField(NAME(HYP_STR(TF_R10_G10_B10_A2)), TextureFormat::TF_R10G10B10A2),
    StaticField(NAME(HYP_STR(TF_R16_F)), TextureFormat::TF_R16F),
    StaticField(NAME(HYP_STR(TF_RG16_F)), TextureFormat::TF_RG16F),
    StaticField(NAME(HYP_STR(TF_RGB16_F)), TextureFormat::TF_RGB16F),
    StaticField(NAME(HYP_STR(TF_RGBA16_F)), TextureFormat::TF_RGBA16F),
    StaticField(NAME(HYP_STR(TF_R32_F)), TextureFormat::TF_R32F),
    StaticField(NAME(HYP_STR(TF_RG32_F)), TextureFormat::TF_RG32F),
    StaticField(NAME(HYP_STR(TF_RGB32_F)), TextureFormat::TF_RGB32F),
    StaticField(NAME(HYP_STR(TF_RGBA32_F)), TextureFormat::TF_RGBA32F),
    StaticField(NAME(HYP_STR(TF_SRGB)), TextureFormat::TF_SRGB),
    StaticField(NAME(HYP_STR(TF_R8_SRGB)), TextureFormat::TF_R8_SRGB),
    StaticField(NAME(HYP_STR(TF_RG8_SRGB)), TextureFormat::TF_RG8_SRGB),
    StaticField(NAME(HYP_STR(TF_RGB8_SRGB)), TextureFormat::TF_RGB8_SRGB),
    StaticField(NAME(HYP_STR(TF_RGBA8_SRGB)), TextureFormat::TF_RGBA8_SRGB),
    StaticField(NAME(HYP_STR(TF_B8_SRGB)), TextureFormat::TF_B8_SRGB),
    StaticField(NAME(HYP_STR(TF_BG8_SRGB)), TextureFormat::TF_BG8_SRGB),
    StaticField(NAME(HYP_STR(TF_BGR8_SRGB)), TextureFormat::TF_BGR8_SRGB),
    StaticField(NAME(HYP_STR(TF_BGRA8_SRGB)), TextureFormat::TF_BGRA8_SRGB),
    StaticField(NAME(HYP_STR(TF_DEPTH)), TextureFormat::TF_DEPTH),
    StaticField(NAME(HYP_STR(TF_DEPTH_16)), TextureFormat::TF_DEPTH_16),
    StaticField(NAME(HYP_STR(TF_DEPTH_24)), TextureFormat::TF_DEPTH_24),
    StaticField(NAME(HYP_STR(TF_DEPTH_32_F)), TextureFormat::TF_DEPTH_32F)
HYP_END_ENUM

#pragma endregion TextureFormat Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region FaceCullMode Reflection Data

HYP_BEGIN_ENUM(FaceCullMode, 316, 0, {})
    StaticField(NAME(HYP_STR(FCM_NONE)), FaceCullMode::FCM_NONE),
    StaticField(NAME(HYP_STR(FCM_BACK)), FaceCullMode::FCM_BACK),
    StaticField(NAME(HYP_STR(FCM_FRONT)), FaceCullMode::FCM_FRONT)
HYP_END_ENUM

#pragma endregion FaceCullMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region StencilFunction Reflection Data

HYP_BEGIN_STRUCT(StencilFunction, 317, 0, {})
    Field(NAME(HYP_STR(PassOp)), &StencilFunction::passOp, offsetof(StencilFunction, passOp), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(FailOp)), &StencilFunction::failOp, offsetof(StencilFunction, failOp), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(DepthFailOp)), &StencilFunction::depthFailOp, offsetof(StencilFunction, depthFailOp), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(CompareOp)), &StencilFunction::compareOp, offsetof(StencilFunction, compareOp), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Mask)), &StencilFunction::mask, offsetof(StencilFunction, mask), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Value)), &StencilFunction::value, offsetof(StencilFunction, value), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion StencilFunction Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureDesc Reflection Data

HYP_BEGIN_STRUCT(TextureDesc, 318, 0, {})
    Field(NAME(HYP_STR(Type)), &TextureDesc::type, offsetof(TextureDesc, type), Span<const ClassAttribute> { {ClassAttribute("property", "Type"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Format)), &TextureDesc::format, offsetof(TextureDesc, format), Span<const ClassAttribute> { {ClassAttribute("property", "Format"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Extent)), &TextureDesc::extent, offsetof(TextureDesc, extent), Span<const ClassAttribute> { {ClassAttribute("property", "Extent"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(FilterModeMin)), &TextureDesc::filterModeMin, offsetof(TextureDesc, filterModeMin), Span<const ClassAttribute> { {ClassAttribute("property", "MinFilterMode"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(FilterModeMag)), &TextureDesc::filterModeMag, offsetof(TextureDesc, filterModeMag), Span<const ClassAttribute> { {ClassAttribute("property", "MagFilterMode"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(WrapMode)), &TextureDesc::wrapMode, offsetof(TextureDesc, wrapMode), Span<const ClassAttribute> { {ClassAttribute("property", "TextureWrapMode"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(NumLayers)), &TextureDesc::numLayers, offsetof(TextureDesc, numLayers), Span<const ClassAttribute> { {ClassAttribute("property", "NumLayers"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(ImageUsage)), &TextureDesc::imageUsage, offsetof(TextureDesc, imageUsage), Span<const ClassAttribute> { {ClassAttribute("property", "ImageUsage"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion TextureDesc Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region LoadOperation Reflection Data

HYP_BEGIN_ENUM(LoadOperation, 319, 0, {})
    StaticField(NAME(HYP_STR(UNDEFINED)), LoadOperation::UNDEFINED),
    StaticField(NAME(HYP_STR(NONE)), LoadOperation::NONE),
    StaticField(NAME(HYP_STR(CLEAR)), LoadOperation::CLEAR),
    StaticField(NAME(HYP_STR(LOAD)), LoadOperation::LOAD)
HYP_END_ENUM

#pragma endregion LoadOperation Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureFilterMode Reflection Data

HYP_BEGIN_ENUM(TextureFilterMode, 320, 0, {})
    StaticField(NAME(HYP_STR(TFM_NEAREST)), TextureFilterMode::TFM_NEAREST),
    StaticField(NAME(HYP_STR(TFM_LINEAR)), TextureFilterMode::TFM_LINEAR),
    StaticField(NAME(HYP_STR(TFM_NEAREST_LINEAR)), TextureFilterMode::TFM_NEAREST_LINEAR),
    StaticField(NAME(HYP_STR(TFM_NEAREST_MIPMAP)), TextureFilterMode::TFM_NEAREST_MIPMAP),
    StaticField(NAME(HYP_STR(TFM_LINEAR_MIPMAP)), TextureFilterMode::TFM_LINEAR_MIPMAP),
    StaticField(NAME(HYP_STR(TFM_MINMAX_MIPMAP)), TextureFilterMode::TFM_MINMAX_MIPMAP)
HYP_END_ENUM

#pragma endregion TextureFilterMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region RenderPassStage Reflection Data

HYP_BEGIN_ENUM(RenderPassStage, 321, 0, {})
    StaticField(NAME(HYP_STR(NONE)), RenderPassStage::NONE),
    StaticField(NAME(HYP_STR(PRESENT)), RenderPassStage::PRESENT),
    StaticField(NAME(HYP_STR(SHADER)), RenderPassStage::SHADER)
HYP_END_ENUM

#pragma endregion RenderPassStage Reflection Data

} // namespace hyperion

