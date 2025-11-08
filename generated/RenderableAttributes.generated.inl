#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region MaterialAttributeFlags Reflection Data

HYP_BEGIN_ENUM(MaterialAttributeFlags, 341, 0, {})
    StaticField(NAME(HYP_STR(MAF_NONE)), MaterialAttributeFlags::MAF_NONE),
    StaticField(NAME(HYP_STR(MAF_DEPTH_WRITE)), MaterialAttributeFlags::MAF_DEPTH_WRITE),
    StaticField(NAME(HYP_STR(MAF_DEPTH_TEST)), MaterialAttributeFlags::MAF_DEPTH_TEST),
    StaticField(NAME(HYP_STR(MAF_ALPHA_DISCARD)), MaterialAttributeFlags::MAF_ALPHA_DISCARD)
HYP_END_ENUM

#pragma endregion MaterialAttributeFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialAttributes Reflection Data

HYP_BEGIN_STRUCT(MaterialAttributes, 342, 0, {})
    Field(NAME(HYP_STR(ShaderDefinition)), &MaterialAttributes::shaderDefinition, offsetof(MaterialAttributes, shaderDefinition)),
    Field(NAME(HYP_STR(Bucket)), &MaterialAttributes::bucket, offsetof(MaterialAttributes, bucket)),
    Field(NAME(HYP_STR(FillMode)), &MaterialAttributes::fillMode, offsetof(MaterialAttributes, fillMode)),
    Field(NAME(HYP_STR(BlendFunction)), &MaterialAttributes::blendFunction, offsetof(MaterialAttributes, blendFunction)),
    Field(NAME(HYP_STR(CullFaces)), &MaterialAttributes::cullFaces, offsetof(MaterialAttributes, cullFaces)),
    Field(NAME(HYP_STR(Flags)), &MaterialAttributes::flags, offsetof(MaterialAttributes, flags)),
    Field(NAME(HYP_STR(StencilFunction)), &MaterialAttributes::stencilFunction, offsetof(MaterialAttributes, stencilFunction)),
    Field(NAME(HYP_STR(TextureMask)), &MaterialAttributes::textureMask, offsetof(MaterialAttributes, textureMask), Span<const ClassAttribute> { {ClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion MaterialAttributes Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MeshAttributes Reflection Data

HYP_BEGIN_STRUCT(MeshAttributes, 343, 0, {})
    Field(NAME(HYP_STR(VertexAttributes)), &MeshAttributes::vertexAttributes, offsetof(MeshAttributes, vertexAttributes), Span<const ClassAttribute> { {ClassAttribute("property", "VertexAttributes") } }),
    Field(NAME(HYP_STR(Topology)), &MeshAttributes::topology, offsetof(MeshAttributes, topology), Span<const ClassAttribute> { {ClassAttribute("property", "Topology") } }),
    Field(NAME(HYP_STR(IndexBufferElemType)), &MeshAttributes::indexBufferElemType, offsetof(MeshAttributes, indexBufferElemType), Span<const ClassAttribute> { {ClassAttribute("property", "IndexBufferElemType") } })
HYP_END_STRUCT

#pragma endregion MeshAttributes Reflection Data

} // namespace hyperion

