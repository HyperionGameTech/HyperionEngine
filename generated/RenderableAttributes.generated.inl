#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region MaterialAttributeFlags Reflection Data

HYP_BEGIN_ENUM(MaterialAttributeFlags, 329, 0, {})
    HypConstant(NAME(HYP_STR(MAF_NONE)), MaterialAttributeFlags::MAF_NONE),
    HypConstant(NAME(HYP_STR(MAF_DEPTH_WRITE)), MaterialAttributeFlags::MAF_DEPTH_WRITE),
    HypConstant(NAME(HYP_STR(MAF_DEPTH_TEST)), MaterialAttributeFlags::MAF_DEPTH_TEST),
    HypConstant(NAME(HYP_STR(MAF_ALPHA_DISCARD)), MaterialAttributeFlags::MAF_ALPHA_DISCARD)
HYP_END_ENUM

#pragma endregion MaterialAttributeFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialAttributes Reflection Data

HYP_BEGIN_STRUCT(MaterialAttributes, 330, 0, {})
    HypField(NAME(HYP_STR(ShaderDefinition)), &MaterialAttributes::shaderDefinition, offsetof(MaterialAttributes, shaderDefinition), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Bucket)), &MaterialAttributes::bucket, offsetof(MaterialAttributes, bucket), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(FillMode)), &MaterialAttributes::fillMode, offsetof(MaterialAttributes, fillMode), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(BlendFunction)), &MaterialAttributes::blendFunction, offsetof(MaterialAttributes, blendFunction), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(CullFaces)), &MaterialAttributes::cullFaces, offsetof(MaterialAttributes, cullFaces), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Flags)), &MaterialAttributes::flags, offsetof(MaterialAttributes, flags), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(StencilFunction)), &MaterialAttributes::stencilFunction, offsetof(MaterialAttributes, stencilFunction), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MaterialAttributes Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MeshAttributes Reflection Data

HYP_BEGIN_STRUCT(MeshAttributes, 331, 0, {})
    HypField(NAME(HYP_STR(VertexAttributes)), &MeshAttributes::vertexAttributes, offsetof(MeshAttributes, vertexAttributes), Span<const HypClassAttribute> { {HypClassAttribute("property", "VertexAttributes"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Topology)), &MeshAttributes::topology, offsetof(MeshAttributes, topology), Span<const HypClassAttribute> { {HypClassAttribute("property", "Topology"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(IndexBufferElemType)), &MeshAttributes::indexBufferElemType, offsetof(MeshAttributes, indexBufferElemType), Span<const HypClassAttribute> { {HypClassAttribute("property", "IndexBufferElemType"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MeshAttributes Reflection Data

} // namespace hyperion

