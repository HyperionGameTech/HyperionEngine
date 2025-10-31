#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region DescriptorSetElementType Reflection Data

HYP_BEGIN_ENUM(DescriptorSetElementType, 283, 0, {})
    HypConstant(NAME(HYP_STR(UNSET)), DescriptorSetElementType::UNSET),
    HypConstant(NAME(HYP_STR(UNIFORM_BUFFER)), DescriptorSetElementType::UNIFORM_BUFFER),
    HypConstant(NAME(HYP_STR(UNIFORM_BUFFER_DYNAMIC)), DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC),
    HypConstant(NAME(HYP_STR(SSBO)), DescriptorSetElementType::SSBO),
    HypConstant(NAME(HYP_STR(STORAGE_BUFFER_DYNAMIC)), DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC),
    HypConstant(NAME(HYP_STR(IMAGE)), DescriptorSetElementType::IMAGE),
    HypConstant(NAME(HYP_STR(IMAGE_STORAGE)), DescriptorSetElementType::IMAGE_STORAGE),
    HypConstant(NAME(HYP_STR(SAMPLER)), DescriptorSetElementType::SAMPLER),
    HypConstant(NAME(HYP_STR(TLAS)), DescriptorSetElementType::TLAS),
    HypConstant(NAME(HYP_STR(MAX)), DescriptorSetElementType::MAX)
HYP_END_ENUM

#pragma endregion DescriptorSetElementType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorTableDeclaration Reflection Data

HYP_BEGIN_STRUCT(DescriptorTableDeclaration, 284, 0, {})
    HypField(NAME(HYP_STR(Elements)), &DescriptorTableDeclaration::elements, offsetof(DescriptorTableDeclaration, elements), Span<const HypClassAttribute> { {HypClassAttribute("property", "Elements"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion DescriptorTableDeclaration Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorTableBase Reflection Data

HYP_BEGIN_CLASS(DescriptorTableBase, 79, 1, NAME("HypObjectBase"), HypClassAttribute("abstract", true),HypClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion DescriptorTableBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetOffsetMap Reflection Data

HYP_BEGIN_STRUCT(DescriptorSetOffsetMap, 285, 0, {})
HYP_END_STRUCT

#pragma endregion DescriptorSetOffsetMap Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorDeclaration Reflection Data

HYP_BEGIN_STRUCT(DescriptorDeclaration, 286, 0, {})
    HypField(NAME(HYP_STR(Slot)), &DescriptorDeclaration::slot, offsetof(DescriptorDeclaration, slot), Span<const HypClassAttribute> { {HypClassAttribute("property", "Slot"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Name)), &DescriptorDeclaration::name, offsetof(DescriptorDeclaration, name), Span<const HypClassAttribute> { {HypClassAttribute("property", "Name"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Count)), &DescriptorDeclaration::count, offsetof(DescriptorDeclaration, count), Span<const HypClassAttribute> { {HypClassAttribute("property", "Count"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Size)), &DescriptorDeclaration::size, offsetof(DescriptorDeclaration, size), Span<const HypClassAttribute> { {HypClassAttribute("property", "Size"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(IsDynamic)), &DescriptorDeclaration::isDynamic, offsetof(DescriptorDeclaration, isDynamic), Span<const HypClassAttribute> { {HypClassAttribute("property", "IsDynamic"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Index)), &DescriptorDeclaration::index, offsetof(DescriptorDeclaration, index), Span<const HypClassAttribute> { {HypClassAttribute("property", "Index"), HypClassAttribute("transient", true), HypClassAttribute("serialize", false) } })
HYP_END_STRUCT

#pragma endregion DescriptorDeclaration Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetDeclaration Reflection Data

HYP_BEGIN_STRUCT(DescriptorSetDeclaration, 287, 0, {})
    HypField(NAME(HYP_STR(SetIndex)), &DescriptorSetDeclaration::setIndex, offsetof(DescriptorSetDeclaration, setIndex), Span<const HypClassAttribute> { {HypClassAttribute("property", "SetIndex"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Name)), &DescriptorSetDeclaration::name, offsetof(DescriptorSetDeclaration, name), Span<const HypClassAttribute> { {HypClassAttribute("property", "Name"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Slots)), &DescriptorSetDeclaration::slots, offsetof(DescriptorSetDeclaration, slots), Span<const HypClassAttribute> { {HypClassAttribute("property", "Slots"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Flags)), &DescriptorSetDeclaration::flags, offsetof(DescriptorSetDeclaration, flags), Span<const HypClassAttribute> { {HypClassAttribute("property", "Flags"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion DescriptorSetDeclaration Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetBase Reflection Data

HYP_BEGIN_CLASS(DescriptorSetBase, 81, 1, NAME("HypObjectBase"), HypClassAttribute("abstract", true),HypClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion DescriptorSetBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetDeclarationFlags Reflection Data

HYP_BEGIN_ENUM(DescriptorSetDeclarationFlags, 288, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), DescriptorSetDeclarationFlags::NONE),
    HypConstant(NAME(HYP_STR(REFERENCE)), DescriptorSetDeclarationFlags::REFERENCE),
    HypConstant(NAME(HYP_STR(TEMPLATE)), DescriptorSetDeclarationFlags::TEMPLATE)
HYP_END_ENUM

#pragma endregion DescriptorSetDeclarationFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSlot Reflection Data

HYP_BEGIN_ENUM(DescriptorSlot, 289, 0, {})
    HypConstant(NAME(HYP_STR(DESCRIPTOR_SLOT_NONE)), DescriptorSlot::DESCRIPTOR_SLOT_NONE),
    HypConstant(NAME(HYP_STR(DESCRIPTOR_SLOT_SRV)), DescriptorSlot::DESCRIPTOR_SLOT_SRV),
    HypConstant(NAME(HYP_STR(DESCRIPTOR_SLOT_UAV)), DescriptorSlot::DESCRIPTOR_SLOT_UAV),
    HypConstant(NAME(HYP_STR(DESCRIPTOR_SLOT_CBUFF)), DescriptorSlot::DESCRIPTOR_SLOT_CBUFF),
    HypConstant(NAME(HYP_STR(DESCRIPTOR_SLOT_SSBO)), DescriptorSlot::DESCRIPTOR_SLOT_SSBO),
    HypConstant(NAME(HYP_STR(DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE)), DescriptorSlot::DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE),
    HypConstant(NAME(HYP_STR(DESCRIPTOR_SLOT_SAMPLER)), DescriptorSlot::DESCRIPTOR_SLOT_SAMPLER),
    HypConstant(NAME(HYP_STR(DESCRIPTOR_SLOT_MAX)), DescriptorSlot::DESCRIPTOR_SLOT_MAX)
HYP_END_ENUM

#pragma endregion DescriptorSlot Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetLayoutElement Reflection Data

HYP_BEGIN_STRUCT(DescriptorSetLayoutElement, 290, 0, {})
    HypField(NAME(HYP_STR(Type)), &DescriptorSetLayoutElement::type, offsetof(DescriptorSetLayoutElement, type)),
    HypField(NAME(HYP_STR(Binding)), &DescriptorSetLayoutElement::binding, offsetof(DescriptorSetLayoutElement, binding)),
    HypField(NAME(HYP_STR(Count)), &DescriptorSetLayoutElement::count, offsetof(DescriptorSetLayoutElement, count)),
    HypField(NAME(HYP_STR(Size)), &DescriptorSetLayoutElement::size, offsetof(DescriptorSetLayoutElement, size))
HYP_END_STRUCT

#pragma endregion DescriptorSetLayoutElement Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorTableOffsetMap Reflection Data

HYP_BEGIN_STRUCT(DescriptorTableOffsetMap, 291, 0, {})
HYP_END_STRUCT

#pragma endregion DescriptorTableOffsetMap Reflection Data

} // namespace hyperion

