#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region BlendFunction Reflection Data

HYP_BEGIN_STRUCT(BlendFunction, 302, 0, {}, HypClassAttribute("serialize", "bitwise"),HypClassAttribute("size", 4))
HYP_END_STRUCT

#pragma endregion BlendFunction Reflection Data

static_assert(sizeof(BlendFunction) == 4, "Expected sizeof(BlendFunction) to be 4 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region Topology Reflection Data

HYP_BEGIN_ENUM(Topology, 303, 0, {})
    HypConstant(NAME(HYP_STR(TOP_TRIANGLES)), Topology::TOP_TRIANGLES),
    HypConstant(NAME(HYP_STR(TOP_TRIANGLE_FAN)), Topology::TOP_TRIANGLE_FAN),
    HypConstant(NAME(HYP_STR(TOP_TRIANGLE_STRIP)), Topology::TOP_TRIANGLE_STRIP),
    HypConstant(NAME(HYP_STR(TOP_LINES)), Topology::TOP_LINES),
    HypConstant(NAME(HYP_STR(TOP_POINTS)), Topology::TOP_POINTS)
HYP_END_ENUM

#pragma endregion Topology Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region BlendModeFactor Reflection Data

HYP_BEGIN_ENUM(BlendModeFactor, 304, 0, {})
    HypConstant(NAME(HYP_STR(BMF_NONE)), BlendModeFactor::BMF_NONE),
    HypConstant(NAME(HYP_STR(BMF_ONE)), BlendModeFactor::BMF_ONE),
    HypConstant(NAME(HYP_STR(BMF_ZERO)), BlendModeFactor::BMF_ZERO),
    HypConstant(NAME(HYP_STR(BMF_SRC_COLOR)), BlendModeFactor::BMF_SRC_COLOR),
    HypConstant(NAME(HYP_STR(BMF_SRC_ALPHA)), BlendModeFactor::BMF_SRC_ALPHA),
    HypConstant(NAME(HYP_STR(BMF_DST_COLOR)), BlendModeFactor::BMF_DST_COLOR),
    HypConstant(NAME(HYP_STR(BMF_DST_ALPHA)), BlendModeFactor::BMF_DST_ALPHA),
    HypConstant(NAME(HYP_STR(BMF_ONE_MINUS_SRC_COLOR)), BlendModeFactor::BMF_ONE_MINUS_SRC_COLOR),
    HypConstant(NAME(HYP_STR(BMF_ONE_MINUS_SRC_ALPHA)), BlendModeFactor::BMF_ONE_MINUS_SRC_ALPHA),
    HypConstant(NAME(HYP_STR(BMF_ONE_MINUS_DST_COLOR)), BlendModeFactor::BMF_ONE_MINUS_DST_COLOR),
    HypConstant(NAME(HYP_STR(BMF_ONE_MINUS_DST_ALPHA)), BlendModeFactor::BMF_ONE_MINUS_DST_ALPHA),
    HypConstant(NAME(HYP_STR(BMF_MAX)), BlendModeFactor::BMF_MAX)
HYP_END_ENUM

#pragma endregion BlendModeFactor Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ImageUsage Reflection Data

HYP_BEGIN_ENUM(ImageUsage, 305, 0, {})
    HypConstant(NAME(HYP_STR(IU_NONE)), ImageUsage::IU_NONE),
    HypConstant(NAME(HYP_STR(IU_SAMPLED)), ImageUsage::IU_SAMPLED),
    HypConstant(NAME(HYP_STR(IU_STORAGE)), ImageUsage::IU_STORAGE),
    HypConstant(NAME(HYP_STR(IU_ATTACHMENT)), ImageUsage::IU_ATTACHMENT),
    HypConstant(NAME(HYP_STR(IU_BLENDED)), ImageUsage::IU_BLENDED)
HYP_END_ENUM

#pragma endregion ImageUsage Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ResourceState Reflection Data

HYP_BEGIN_ENUM(ResourceState, 306, 0, {})
    HypConstant(NAME(HYP_STR(RS_UNDEFINED)), ResourceState::RS_UNDEFINED),
    HypConstant(NAME(HYP_STR(RS_PRE_INITIALIZED)), ResourceState::RS_PRE_INITIALIZED),
    HypConstant(NAME(HYP_STR(RS_COMMON)), ResourceState::RS_COMMON),
    HypConstant(NAME(HYP_STR(RS_VERTEX_BUFFER)), ResourceState::RS_VERTEX_BUFFER),
    HypConstant(NAME(HYP_STR(RS_CONSTANT_BUFFER)), ResourceState::RS_CONSTANT_BUFFER),
    HypConstant(NAME(HYP_STR(RS_INDEX_BUFFER)), ResourceState::RS_INDEX_BUFFER),
    HypConstant(NAME(HYP_STR(RS_RENDER_TARGET)), ResourceState::RS_RENDER_TARGET),
    HypConstant(NAME(HYP_STR(RS_UNORDERED_ACCESS)), ResourceState::RS_UNORDERED_ACCESS),
    HypConstant(NAME(HYP_STR(RS_DEPTH_STENCIL)), ResourceState::RS_DEPTH_STENCIL),
    HypConstant(NAME(HYP_STR(RS_SHADER_RESOURCE)), ResourceState::RS_SHADER_RESOURCE),
    HypConstant(NAME(HYP_STR(RS_STREAM_OUT)), ResourceState::RS_STREAM_OUT),
    HypConstant(NAME(HYP_STR(RS_INDIRECT_ARG)), ResourceState::RS_INDIRECT_ARG),
    HypConstant(NAME(HYP_STR(RS_COPY_DST)), ResourceState::RS_COPY_DST),
    HypConstant(NAME(HYP_STR(RS_COPY_SRC)), ResourceState::RS_COPY_SRC),
    HypConstant(NAME(HYP_STR(RS_RESOLVE_DST)), ResourceState::RS_RESOLVE_DST),
    HypConstant(NAME(HYP_STR(RS_RESOLVE_SRC)), ResourceState::RS_RESOLVE_SRC),
    HypConstant(NAME(HYP_STR(RS_PRESENT)), ResourceState::RS_PRESENT),
    HypConstant(NAME(HYP_STR(RS_READ_GENERIC)), ResourceState::RS_READ_GENERIC),
    HypConstant(NAME(HYP_STR(RS_PREDICATION)), ResourceState::RS_PREDICATION)
HYP_END_ENUM

#pragma endregion ResourceState Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureFormat Reflection Data

HYP_BEGIN_ENUM(TextureFormat, 307, 0, {})
    HypConstant(NAME(HYP_STR(TF_NONE)), TextureFormat::TF_NONE),
    HypConstant(NAME(HYP_STR(TF_R8)), TextureFormat::TF_R8),
    HypConstant(NAME(HYP_STR(TF_RG8)), TextureFormat::TF_RG8),
    HypConstant(NAME(HYP_STR(TF_RGB8)), TextureFormat::TF_RGB8),
    HypConstant(NAME(HYP_STR(TF_RGBA8)), TextureFormat::TF_RGBA8),
    HypConstant(NAME(HYP_STR(TF_B8)), TextureFormat::TF_B8),
    HypConstant(NAME(HYP_STR(TF_BG8)), TextureFormat::TF_BG8),
    HypConstant(NAME(HYP_STR(TF_BGR8)), TextureFormat::TF_BGR8),
    HypConstant(NAME(HYP_STR(TF_BGRA8)), TextureFormat::TF_BGRA8),
    HypConstant(NAME(HYP_STR(TF_R16)), TextureFormat::TF_R16),
    HypConstant(NAME(HYP_STR(TF_RG16)), TextureFormat::TF_RG16),
    HypConstant(NAME(HYP_STR(TF_RGB16)), TextureFormat::TF_RGB16),
    HypConstant(NAME(HYP_STR(TF_RGBA16)), TextureFormat::TF_RGBA16),
    HypConstant(NAME(HYP_STR(TF_R32)), TextureFormat::TF_R32),
    HypConstant(NAME(HYP_STR(TF_RG32)), TextureFormat::TF_RG32),
    HypConstant(NAME(HYP_STR(TF_RGB32)), TextureFormat::TF_RGB32),
    HypConstant(NAME(HYP_STR(TF_RGBA32)), TextureFormat::TF_RGBA32),
    HypConstant(NAME(HYP_STR(TF_R32_)), TextureFormat::TF_R32_),
    HypConstant(NAME(HYP_STR(TF_RG16_)), TextureFormat::TF_RG16_),
    HypConstant(NAME(HYP_STR(TF_R11_G11_B10_F)), TextureFormat::TF_R11G11B10F),
    HypConstant(NAME(HYP_STR(TF_R10_G10_B10_A2)), TextureFormat::TF_R10G10B10A2),
    HypConstant(NAME(HYP_STR(TF_R16_F)), TextureFormat::TF_R16F),
    HypConstant(NAME(HYP_STR(TF_RG16_F)), TextureFormat::TF_RG16F),
    HypConstant(NAME(HYP_STR(TF_RGB16_F)), TextureFormat::TF_RGB16F),
    HypConstant(NAME(HYP_STR(TF_RGBA16_F)), TextureFormat::TF_RGBA16F),
    HypConstant(NAME(HYP_STR(TF_R32_F)), TextureFormat::TF_R32F),
    HypConstant(NAME(HYP_STR(TF_RG32_F)), TextureFormat::TF_RG32F),
    HypConstant(NAME(HYP_STR(TF_RGB32_F)), TextureFormat::TF_RGB32F),
    HypConstant(NAME(HYP_STR(TF_RGBA32_F)), TextureFormat::TF_RGBA32F),
    HypConstant(NAME(HYP_STR(TF_SRGB)), TextureFormat::TF_SRGB),
    HypConstant(NAME(HYP_STR(TF_R8_SRGB)), TextureFormat::TF_R8_SRGB),
    HypConstant(NAME(HYP_STR(TF_RG8_SRGB)), TextureFormat::TF_RG8_SRGB),
    HypConstant(NAME(HYP_STR(TF_RGB8_SRGB)), TextureFormat::TF_RGB8_SRGB),
    HypConstant(NAME(HYP_STR(TF_RGBA8_SRGB)), TextureFormat::TF_RGBA8_SRGB),
    HypConstant(NAME(HYP_STR(TF_B8_SRGB)), TextureFormat::TF_B8_SRGB),
    HypConstant(NAME(HYP_STR(TF_BG8_SRGB)), TextureFormat::TF_BG8_SRGB),
    HypConstant(NAME(HYP_STR(TF_BGR8_SRGB)), TextureFormat::TF_BGR8_SRGB),
    HypConstant(NAME(HYP_STR(TF_BGRA8_SRGB)), TextureFormat::TF_BGRA8_SRGB),
    HypConstant(NAME(HYP_STR(TF_DEPTH)), TextureFormat::TF_DEPTH),
    HypConstant(NAME(HYP_STR(TF_DEPTH_16)), TextureFormat::TF_DEPTH_16),
    HypConstant(NAME(HYP_STR(TF_DEPTH_24)), TextureFormat::TF_DEPTH_24),
    HypConstant(NAME(HYP_STR(TF_DEPTH_32_F)), TextureFormat::TF_DEPTH_32F)
HYP_END_ENUM

#pragma endregion TextureFormat Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GpuElemType Reflection Data

HYP_BEGIN_ENUM(GpuElemType, 308, 0, {})
    HypConstant(NAME(HYP_STR(GET_UNSIGNED_BYTE)), GpuElemType::GET_UNSIGNED_BYTE),
    HypConstant(NAME(HYP_STR(GET_SIGNED_BYTE)), GpuElemType::GET_SIGNED_BYTE),
    HypConstant(NAME(HYP_STR(GET_UNSIGNED_SHORT)), GpuElemType::GET_UNSIGNED_SHORT),
    HypConstant(NAME(HYP_STR(GET_SIGNED_SHORT)), GpuElemType::GET_SIGNED_SHORT),
    HypConstant(NAME(HYP_STR(GET_UNSIGNED_INT)), GpuElemType::GET_UNSIGNED_INT),
    HypConstant(NAME(HYP_STR(GET_SIGNED_INT)), GpuElemType::GET_SIGNED_INT),
    HypConstant(NAME(HYP_STR(GET_FLOAT)), GpuElemType::GET_FLOAT),
    HypConstant(NAME(HYP_STR(GET_MAX)), GpuElemType::GET_MAX)
HYP_END_ENUM

#pragma endregion GpuElemType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region StencilOp Reflection Data

HYP_BEGIN_ENUM(StencilOp, 309, 0, {})
    HypConstant(NAME(HYP_STR(SO_KEEP)), StencilOp::SO_KEEP),
    HypConstant(NAME(HYP_STR(SO_ZERO)), StencilOp::SO_ZERO),
    HypConstant(NAME(HYP_STR(SO_REPLACE)), StencilOp::SO_REPLACE),
    HypConstant(NAME(HYP_STR(SO_INCREMENT)), StencilOp::SO_INCREMENT),
    HypConstant(NAME(HYP_STR(SO_DECREMENT)), StencilOp::SO_DECREMENT)
HYP_END_ENUM

#pragma endregion StencilOp Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureWrapMode Reflection Data

HYP_BEGIN_ENUM(TextureWrapMode, 310, 0, {})
    HypConstant(NAME(HYP_STR(TWM_CLAMP_TO_EDGE)), TextureWrapMode::TWM_CLAMP_TO_EDGE),
    HypConstant(NAME(HYP_STR(TWM_CLAMP_TO_BORDER)), TextureWrapMode::TWM_CLAMP_TO_BORDER),
    HypConstant(NAME(HYP_STR(TWM_REPEAT)), TextureWrapMode::TWM_REPEAT)
HYP_END_ENUM

#pragma endregion TextureWrapMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region FaceCullMode Reflection Data

HYP_BEGIN_ENUM(FaceCullMode, 311, 0, {})
    HypConstant(NAME(HYP_STR(FCM_NONE)), FaceCullMode::FCM_NONE),
    HypConstant(NAME(HYP_STR(FCM_BACK)), FaceCullMode::FCM_BACK),
    HypConstant(NAME(HYP_STR(FCM_FRONT)), FaceCullMode::FCM_FRONT)
HYP_END_ENUM

#pragma endregion FaceCullMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region StencilFunction Reflection Data

HYP_BEGIN_STRUCT(StencilFunction, 312, 0, {})
    HypField(NAME(HYP_STR(PassOp)), &StencilFunction::passOp, offsetof(StencilFunction, passOp), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(FailOp)), &StencilFunction::failOp, offsetof(StencilFunction, failOp), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(DepthFailOp)), &StencilFunction::depthFailOp, offsetof(StencilFunction, depthFailOp), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(CompareOp)), &StencilFunction::compareOp, offsetof(StencilFunction, compareOp), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Mask)), &StencilFunction::mask, offsetof(StencilFunction, mask), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Value)), &StencilFunction::value, offsetof(StencilFunction, value), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion StencilFunction Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureDesc Reflection Data

HYP_BEGIN_STRUCT(TextureDesc, 313, 0, {})
    HypField(NAME(HYP_STR(Type)), &TextureDesc::type, offsetof(TextureDesc, type), Span<const HypClassAttribute> { {HypClassAttribute("property", "Type"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Format)), &TextureDesc::format, offsetof(TextureDesc, format), Span<const HypClassAttribute> { {HypClassAttribute("property", "Format"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Extent)), &TextureDesc::extent, offsetof(TextureDesc, extent), Span<const HypClassAttribute> { {HypClassAttribute("property", "Extent"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(FilterModeMin)), &TextureDesc::filterModeMin, offsetof(TextureDesc, filterModeMin), Span<const HypClassAttribute> { {HypClassAttribute("property", "MinFilterMode"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(FilterModeMag)), &TextureDesc::filterModeMag, offsetof(TextureDesc, filterModeMag), Span<const HypClassAttribute> { {HypClassAttribute("property", "MagFilterMode"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(WrapMode)), &TextureDesc::wrapMode, offsetof(TextureDesc, wrapMode), Span<const HypClassAttribute> { {HypClassAttribute("property", "TextureWrapMode"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(NumLayers)), &TextureDesc::numLayers, offsetof(TextureDesc, numLayers), Span<const HypClassAttribute> { {HypClassAttribute("property", "NumLayers"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(ImageUsage)), &TextureDesc::imageUsage, offsetof(TextureDesc, imageUsage), Span<const HypClassAttribute> { {HypClassAttribute("property", "ImageUsage"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion TextureDesc Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region StencilCompareOp Reflection Data

HYP_BEGIN_ENUM(StencilCompareOp, 314, 0, {})
    HypConstant(NAME(HYP_STR(SCO_ALWAYS)), StencilCompareOp::SCO_ALWAYS),
    HypConstant(NAME(HYP_STR(SCO_NEVER)), StencilCompareOp::SCO_NEVER),
    HypConstant(NAME(HYP_STR(SCO_EQUAL)), StencilCompareOp::SCO_EQUAL),
    HypConstant(NAME(HYP_STR(SCO_NOT_EQUAL)), StencilCompareOp::SCO_NOT_EQUAL)
HYP_END_ENUM

#pragma endregion StencilCompareOp Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ImageSupport Reflection Data

HYP_BEGIN_ENUM(ImageSupport, 315, 0, {})
    HypConstant(NAME(HYP_STR(IS_SRV)), ImageSupport::IS_SRV),
    HypConstant(NAME(HYP_STR(IS_UAV)), ImageSupport::IS_UAV),
    HypConstant(NAME(HYP_STR(IS_DEPTH)), ImageSupport::IS_DEPTH)
HYP_END_ENUM

#pragma endregion ImageSupport Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region FillMode Reflection Data

HYP_BEGIN_ENUM(FillMode, 316, 0, {})
    HypConstant(NAME(HYP_STR(FM_FILL)), FillMode::FM_FILL),
    HypConstant(NAME(HYP_STR(FM_LINE)), FillMode::FM_LINE)
HYP_END_ENUM

#pragma endregion FillMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DefaultImageFormat Reflection Data

HYP_BEGIN_ENUM(DefaultImageFormat, 317, 0, {})
    HypConstant(NAME(HYP_STR(DIF_NONE)), DefaultImageFormat::DIF_NONE),
    HypConstant(NAME(HYP_STR(DIF_COLOR)), DefaultImageFormat::DIF_COLOR),
    HypConstant(NAME(HYP_STR(DIF_DEPTH)), DefaultImageFormat::DIF_DEPTH),
    HypConstant(NAME(HYP_STR(DIF_NORMALS)), DefaultImageFormat::DIF_NORMALS),
    HypConstant(NAME(HYP_STR(DIF_STORAGE)), DefaultImageFormat::DIF_STORAGE)
HYP_END_ENUM

#pragma endregion DefaultImageFormat Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureType Reflection Data

HYP_BEGIN_ENUM(TextureType, 318, 0, {})
    HypConstant(NAME(HYP_STR(TT_INVALID)), TextureType::TT_INVALID),
    HypConstant(NAME(HYP_STR(TT_TEX2_D)), TextureType::TT_TEX2D),
    HypConstant(NAME(HYP_STR(TT_TEX3_D)), TextureType::TT_TEX3D),
    HypConstant(NAME(HYP_STR(TT_CUBEMAP)), TextureType::TT_CUBEMAP),
    HypConstant(NAME(HYP_STR(TT_TEX2_D_ARRAY)), TextureType::TT_TEX2D_ARRAY),
    HypConstant(NAME(HYP_STR(TT_CUBEMAP_ARRAY)), TextureType::TT_CUBEMAP_ARRAY),
    HypConstant(NAME(HYP_STR(TT_MAX)), TextureType::TT_MAX)
HYP_END_ENUM

#pragma endregion TextureType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureData Reflection Data

HYP_BEGIN_STRUCT(TextureData, 319, 0, {})
    HypField(NAME(HYP_STR(ImageData)), &TextureData::imageData, offsetof(TextureData, imageData), Span<const HypClassAttribute> { {HypClassAttribute("property", "ImageData"), HypClassAttribute("serialize", true), HypClassAttribute("compressed", true) } })
HYP_END_STRUCT

#pragma endregion TextureData Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureFilterMode Reflection Data

HYP_BEGIN_ENUM(TextureFilterMode, 320, 0, {})
    HypConstant(NAME(HYP_STR(TFM_NEAREST)), TextureFilterMode::TFM_NEAREST),
    HypConstant(NAME(HYP_STR(TFM_LINEAR)), TextureFilterMode::TFM_LINEAR),
    HypConstant(NAME(HYP_STR(TFM_NEAREST_LINEAR)), TextureFilterMode::TFM_NEAREST_LINEAR),
    HypConstant(NAME(HYP_STR(TFM_NEAREST_MIPMAP)), TextureFilterMode::TFM_NEAREST_MIPMAP),
    HypConstant(NAME(HYP_STR(TFM_LINEAR_MIPMAP)), TextureFilterMode::TFM_LINEAR_MIPMAP),
    HypConstant(NAME(HYP_STR(TFM_MINMAX_MIPMAP)), TextureFilterMode::TFM_MINMAX_MIPMAP)
HYP_END_ENUM

#pragma endregion TextureFilterMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextureBaseFormat Reflection Data

HYP_BEGIN_ENUM(TextureBaseFormat, 321, 0, {})
    HypConstant(NAME(HYP_STR(TFB_NONE)), TextureBaseFormat::TFB_NONE),
    HypConstant(NAME(HYP_STR(TFB_R)), TextureBaseFormat::TFB_R),
    HypConstant(NAME(HYP_STR(TFB_RG)), TextureBaseFormat::TFB_RG),
    HypConstant(NAME(HYP_STR(TFB_RGB)), TextureBaseFormat::TFB_RGB),
    HypConstant(NAME(HYP_STR(TFB_RGBA)), TextureBaseFormat::TFB_RGBA),
    HypConstant(NAME(HYP_STR(TFB_BGR)), TextureBaseFormat::TFB_BGR),
    HypConstant(NAME(HYP_STR(TFB_BGRA)), TextureBaseFormat::TFB_BGRA),
    HypConstant(NAME(HYP_STR(TFB_DEPTH)), TextureBaseFormat::TFB_DEPTH)
HYP_END_ENUM

#pragma endregion TextureBaseFormat Reflection Data

} // namespace hyperion

