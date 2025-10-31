#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region GpuBufferType Reflection Data

HYP_BEGIN_ENUM(GpuBufferType, 324, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), GpuBufferType::NONE),
    HypConstant(NAME(HYP_STR(MESH_INDEX_BUFFER)), GpuBufferType::MESH_INDEX_BUFFER),
    HypConstant(NAME(HYP_STR(MESH_VERTEX_BUFFER)), GpuBufferType::MESH_VERTEX_BUFFER),
    HypConstant(NAME(HYP_STR(CBUFF)), GpuBufferType::CBUFF),
    HypConstant(NAME(HYP_STR(SSBO)), GpuBufferType::SSBO),
    HypConstant(NAME(HYP_STR(ATOMIC_COUNTER)), GpuBufferType::ATOMIC_COUNTER),
    HypConstant(NAME(HYP_STR(STAGING_BUFFER)), GpuBufferType::STAGING_BUFFER),
    HypConstant(NAME(HYP_STR(INDIRECT_ARGS_BUFFER)), GpuBufferType::INDIRECT_ARGS_BUFFER),
    HypConstant(NAME(HYP_STR(SHADER_BINDING_TABLE)), GpuBufferType::SHADER_BINDING_TABLE),
    HypConstant(NAME(HYP_STR(ACCELERATION_STRUCTURE_BUFFER)), GpuBufferType::ACCELERATION_STRUCTURE_BUFFER),
    HypConstant(NAME(HYP_STR(ACCELERATION_STRUCTURE_INSTANCE_BUFFER)), GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER),
    HypConstant(NAME(HYP_STR(RT_MESH_INDEX_BUFFER)), GpuBufferType::RT_MESH_INDEX_BUFFER),
    HypConstant(NAME(HYP_STR(RT_MESH_VERTEX_BUFFER)), GpuBufferType::RT_MESH_VERTEX_BUFFER),
    HypConstant(NAME(HYP_STR(SCRATCH_BUFFER)), GpuBufferType::SCRATCH_BUFFER),
    HypConstant(NAME(HYP_STR(MAX)), GpuBufferType::MAX)
HYP_END_ENUM

#pragma endregion GpuBufferType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GpuBufferBase Reflection Data

HYP_BEGIN_CLASS(GpuBufferBase, 99, 1, NAME("HypObjectBase"), HypClassAttribute("abstract", true),HypClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion GpuBufferBase Reflection Data

} // namespace hyperion

