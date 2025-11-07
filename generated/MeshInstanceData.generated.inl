#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region MeshInstanceData Reflection Data

HYP_BEGIN_STRUCT(MeshInstanceData, 329, 0, {}, ClassAttribute("postload", "MeshInstanceData_OnPostLoad"),ClassAttribute("size", 88))
    Field(NAME(HYP_STR(NumInstances)), &MeshInstanceData::numInstances, offsetof(MeshInstanceData, numInstances), Span<const ClassAttribute> { {ClassAttribute("property", "NumInstances"), ClassAttribute("serialize", true), ClassAttribute("editor", true), ClassAttribute("description", "The number of instances of this mesh. This is used to determine how many instances to render in a single draw call. If this is set to 1, the mesh will be rendered as a single instance. If this is greater than 1, the mesh will be rendered as multiple instances.") } }),
    Field(NAME(HYP_STR(EnableAutoInstancing)), &MeshInstanceData::enableAutoInstancing, offsetof(MeshInstanceData, enableAutoInstancing), Span<const ClassAttribute> { {ClassAttribute("property", "EnableAutoInstancing"), ClassAttribute("serialize", true), ClassAttribute("editor", true), ClassAttribute("description", "Enable automatic instancing for this mesh instance data. If enabled, the renderer will automatically batch instances of this mesh together for rendering, regardless of the explicitly set number of instances. This can improve performance by reducing draw calls for duplicate meshes, but may consume more GPU memory if instancing is under utilized for this mesh.") } }),
    Field(NAME(HYP_STR(Buffers)), &MeshInstanceData::buffers, offsetof(MeshInstanceData, buffers), Span<const ClassAttribute> { {ClassAttribute("property", "Buffers"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(BufferStructSizes)), &MeshInstanceData::bufferStructSizes, offsetof(MeshInstanceData, bufferStructSizes), Span<const ClassAttribute> { {ClassAttribute("property", "BufferStructSizes"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(BufferStructAlignments)), &MeshInstanceData::bufferStructAlignments, offsetof(MeshInstanceData, bufferStructAlignments), Span<const ClassAttribute> { {ClassAttribute("property", "BufferStructAlignments"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MeshInstanceData Reflection Data

static_assert(sizeof(MeshInstanceData) == 88, "Expected sizeof(MeshInstanceData) to be 88 bytes");
static const ClassCallbackRegistration<ClassCallbackType::ON_POST_LOAD> g_post_load_MeshInstanceData(TypeId::ForType<MeshInstanceData>(), ValueWrapper<MeshInstanceData_OnPostLoad>());
} // namespace hyperion

