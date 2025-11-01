#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region DescriptorUsage Reflection Data

HYP_BEGIN_STRUCT(DescriptorUsage, 331, 0, {})
    HypField(NAME(HYP_STR(Slot)), &DescriptorUsage::slot, offsetof(DescriptorUsage, slot), Span<const HypClassAttribute> { {HypClassAttribute("property", "Slot"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(SetName)), &DescriptorUsage::setName, offsetof(DescriptorUsage, setName), Span<const HypClassAttribute> { {HypClassAttribute("property", "SetName"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(DescriptorName)), &DescriptorUsage::descriptorName, offsetof(DescriptorUsage, descriptorName), Span<const HypClassAttribute> { {HypClassAttribute("property", "DescriptorName"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Type)), &DescriptorUsage::type, offsetof(DescriptorUsage, type), Span<const HypClassAttribute> { {HypClassAttribute("property", "Type"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Flags)), &DescriptorUsage::flags, offsetof(DescriptorUsage, flags), Span<const HypClassAttribute> { {HypClassAttribute("property", "Flags"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Params)), &DescriptorUsage::params, offsetof(DescriptorUsage, params), Span<const HypClassAttribute> { {HypClassAttribute("property", "Params"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion DescriptorUsage Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region CompiledShader Reflection Data

HYP_BEGIN_STRUCT(CompiledShader, 332, 0, {})
    HypField(NAME(HYP_STR(Definition)), &CompiledShader::definition, offsetof(CompiledShader, definition), Span<const HypClassAttribute> { {HypClassAttribute("property", "Definition") } }),
    HypField(NAME(HYP_STR(DescriptorTableDeclaration)), &CompiledShader::descriptorTableDeclaration, offsetof(CompiledShader, descriptorTableDeclaration), Span<const HypClassAttribute> { {HypClassAttribute("property", "DescriptorTableDeclaration"), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(DescriptorUsageSet)), &CompiledShader::descriptorUsageSet, offsetof(CompiledShader, descriptorUsageSet), Span<const HypClassAttribute> { {HypClassAttribute("property", "DescriptorUsageSet") } }),
    HypField(NAME(HYP_STR(EntryPointName)), &CompiledShader::entryPointName, offsetof(CompiledShader, entryPointName), Span<const HypClassAttribute> { {HypClassAttribute("property", "EntryPointName") } }),
    HypField(NAME(HYP_STR(Modules)), &CompiledShader::modules, offsetof(CompiledShader, modules), Span<const HypClassAttribute> { {HypClassAttribute("property", "Modules") } }),
    HypMethod(NAME(HYP_STR(GetRevisionNumber)), &CompiledShader::GetRevisionNumber, Span<const HypClassAttribute> { {HypClassAttribute("property", "RevisionNumber"), HypClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion CompiledShader Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShaderPropertyFlags Reflection Data

HYP_BEGIN_ENUM(ShaderPropertyFlags, 333, 0, {})
    HypConstant(NAME(HYP_STR(SPF_NONE)), ShaderPropertyFlags::SPF_NONE),
    HypConstant(NAME(HYP_STR(SPF_VERTEX_ATTRIBUTE)), ShaderPropertyFlags::SPF_VERTEX_ATTRIBUTE),
    HypConstant(NAME(HYP_STR(SPF_PERMUTATION)), ShaderPropertyFlags::SPF_PERMUTATION)
HYP_END_ENUM

#pragma endregion ShaderPropertyFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShaderDefinition Reflection Data

HYP_BEGIN_STRUCT(ShaderDefinition, 334, 0, {})
    HypField(NAME(HYP_STR(Name)), &ShaderDefinition::name, offsetof(ShaderDefinition, name)),
    HypField(NAME(HYP_STR(Properties)), &ShaderDefinition::properties, offsetof(ShaderDefinition, properties))
HYP_END_STRUCT

#pragma endregion ShaderDefinition Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ProcessShaderSourcePhase Reflection Data

HYP_BEGIN_ENUM(ProcessShaderSourcePhase, 335, 0, {})
    HypConstant(NAME(HYP_STR(BEFORE_PREPROCESS)), ProcessShaderSourcePhase::BEFORE_PREPROCESS),
    HypConstant(NAME(HYP_STR(AFTER_PREPROCESS)), ProcessShaderSourcePhase::AFTER_PREPROCESS)
HYP_END_ENUM

#pragma endregion ProcessShaderSourcePhase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShaderProperties Reflection Data

HYP_BEGIN_STRUCT(ShaderProperties, 336, 0, {})
    HypField(NAME(HYP_STR(Props)), &ShaderProperties::m_props, offsetof(ShaderProperties, m_props)),
    HypField(NAME(HYP_STR(RequiredVertexAttributes)), &ShaderProperties::m_requiredVertexAttributes, offsetof(ShaderProperties, m_requiredVertexAttributes)),
    HypField(NAME(HYP_STR(OptionalVertexAttributes)), &ShaderProperties::m_optionalVertexAttributes, offsetof(ShaderProperties, m_optionalVertexAttributes))
HYP_END_STRUCT

#pragma endregion ShaderProperties Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShaderProperty Reflection Data

HYP_BEGIN_STRUCT(ShaderProperty, 337, 0, {})
    HypField(NAME(HYP_STR(Name)), &ShaderProperty::name, offsetof(ShaderProperty, name)),
    HypField(NAME(HYP_STR(Flags)), &ShaderProperty::flags, offsetof(ShaderProperty, flags)),
    HypField(NAME(HYP_STR(CurrentValue)), &ShaderProperty::currentValue, offsetof(ShaderProperty, currentValue)),
    HypField(NAME(HYP_STR(EnumValues)), &ShaderProperty::enumValues, offsetof(ShaderProperty, enumValues))
HYP_END_STRUCT

#pragma endregion ShaderProperty Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShaderLanguage Reflection Data

HYP_BEGIN_ENUM(ShaderLanguage, 338, 0, {})
    HypConstant(NAME(HYP_STR(GLSL)), ShaderLanguage::GLSL),
    HypConstant(NAME(HYP_STR(HLSL)), ShaderLanguage::HLSL)
HYP_END_ENUM

#pragma endregion ShaderLanguage Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region VertexAttributeDefinition Reflection Data

HYP_BEGIN_STRUCT(VertexAttributeDefinition, 339, 0, {})
    HypField(NAME(HYP_STR(Name)), &VertexAttributeDefinition::name, offsetof(VertexAttributeDefinition, name)),
    HypField(NAME(HYP_STR(TypeClass)), &VertexAttributeDefinition::typeClass, offsetof(VertexAttributeDefinition, typeClass)),
    HypField(NAME(HYP_STR(Location)), &VertexAttributeDefinition::location, offsetof(VertexAttributeDefinition, location)),
    HypField(NAME(HYP_STR(Condition)), &VertexAttributeDefinition::condition, offsetof(VertexAttributeDefinition, condition))
HYP_END_STRUCT

#pragma endregion VertexAttributeDefinition Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region CompiledShaderBatch Reflection Data

HYP_BEGIN_STRUCT(CompiledShaderBatch, 340, 0, {})
    HypField(NAME(HYP_STR(CompiledShaders)), &CompiledShaderBatch::compiledShaders, offsetof(CompiledShaderBatch, compiledShaders)),
    HypField(NAME(HYP_STR(ErrorMessages)), &CompiledShaderBatch::errorMessages, offsetof(CompiledShaderBatch, errorMessages))
HYP_END_STRUCT

#pragma endregion CompiledShaderBatch Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region HashedShaderDefinition Reflection Data

HYP_BEGIN_STRUCT(HashedShaderDefinition, 341, 0, {})
    HypField(NAME(HYP_STR(Name)), &HashedShaderDefinition::name, offsetof(HashedShaderDefinition, name)),
    HypField(NAME(HYP_STR(PropertySetHash)), &HashedShaderDefinition::propertySetHash, offsetof(HashedShaderDefinition, propertySetHash)),
    HypField(NAME(HYP_STR(RequiredVertexAttributes)), &HashedShaderDefinition::requiredVertexAttributes, offsetof(HashedShaderDefinition, requiredVertexAttributes))
HYP_END_STRUCT

#pragma endregion HashedShaderDefinition Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorUsageSet Reflection Data

HYP_BEGIN_STRUCT(DescriptorUsageSet, 342, 0, {})
    HypField(NAME(HYP_STR(Elements)), &DescriptorUsageSet::elements, offsetof(DescriptorUsageSet, elements))
HYP_END_STRUCT

#pragma endregion DescriptorUsageSet Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorUsageFlags Reflection Data

HYP_BEGIN_ENUM(DescriptorUsageFlags, 343, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), DescriptorUsageFlags::NONE),
    HypConstant(NAME(HYP_STR(DYNAMIC)), DescriptorUsageFlags::DYNAMIC)
HYP_END_ENUM

#pragma endregion DescriptorUsageFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorUsageType Reflection Data

HYP_BEGIN_STRUCT(DescriptorUsageType, 344, 0, {})
    HypField(NAME(HYP_STR(Name)), &DescriptorUsageType::name, offsetof(DescriptorUsageType, name), Span<const HypClassAttribute> { {HypClassAttribute("property", "Name"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Size)), &DescriptorUsageType::size, offsetof(DescriptorUsageType, size), Span<const HypClassAttribute> { {HypClassAttribute("property", "Size"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(FieldNames)), &DescriptorUsageType::fieldNames, offsetof(DescriptorUsageType, fieldNames), Span<const HypClassAttribute> { {HypClassAttribute("property", "FieldNames"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(FieldTypes)), &DescriptorUsageType::fieldTypes, offsetof(DescriptorUsageType, fieldTypes), Span<const HypClassAttribute> { {HypClassAttribute("property", "FieldTypes"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion DescriptorUsageType Reflection Data

} // namespace hyperion

