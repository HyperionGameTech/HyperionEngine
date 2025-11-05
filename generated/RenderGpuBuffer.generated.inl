#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region GpuBufferType Reflection Data

HYP_BEGIN_ENUM(GpuBufferType, 325, 0, {})
    StaticField(NAME(HYP_STR(NONE)), GpuBufferType::NONE),
    StaticField(NAME(HYP_STR(MESH_INDEX_BUFFER)), GpuBufferType::MESH_INDEX_BUFFER),
    StaticField(NAME(HYP_STR(MESH_VERTEX_BUFFER)), GpuBufferType::MESH_VERTEX_BUFFER),
    StaticField(NAME(HYP_STR(CBUFF)), GpuBufferType::CBUFF),
    StaticField(NAME(HYP_STR(SSBO)), GpuBufferType::SSBO),
    StaticField(NAME(HYP_STR(ATOMIC_COUNTER)), GpuBufferType::ATOMIC_COUNTER),
    StaticField(NAME(HYP_STR(STAGING_BUFFER)), GpuBufferType::STAGING_BUFFER),
    StaticField(NAME(HYP_STR(INDIRECT_ARGS_BUFFER)), GpuBufferType::INDIRECT_ARGS_BUFFER),
    StaticField(NAME(HYP_STR(SHADER_BINDING_TABLE)), GpuBufferType::SHADER_BINDING_TABLE),
    StaticField(NAME(HYP_STR(ACCELERATION_STRUCTURE_BUFFER)), GpuBufferType::ACCELERATION_STRUCTURE_BUFFER),
    StaticField(NAME(HYP_STR(ACCELERATION_STRUCTURE_INSTANCE_BUFFER)), GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER),
    StaticField(NAME(HYP_STR(RT_MESH_INDEX_BUFFER)), GpuBufferType::RT_MESH_INDEX_BUFFER),
    StaticField(NAME(HYP_STR(RT_MESH_VERTEX_BUFFER)), GpuBufferType::RT_MESH_VERTEX_BUFFER),
    StaticField(NAME(HYP_STR(SCRATCH_BUFFER)), GpuBufferType::SCRATCH_BUFFER),
    StaticField(NAME(HYP_STR(MAX)), GpuBufferType::MAX)
HYP_END_ENUM

#pragma endregion GpuBufferType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GpuBufferBase Reflection Data

HYP_BEGIN_CLASS(GpuBufferBase, 99, 1, NAME("HypObjectBase"), ClassAttribute("abstract", true),ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion GpuBufferBase Reflection Data

} // namespace hyperion

