#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region DescriptorTableDeclaration Reflection Data

HYP_BEGIN_STRUCT(DescriptorTableDeclaration, 285, 0, {})
    Field(NAME(HYP_STR(Elements)), &DescriptorTableDeclaration::elements, offsetof(DescriptorTableDeclaration, elements), Span<const ClassAttribute> { {ClassAttribute("property", "Elements"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion DescriptorTableDeclaration Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorTableBase Reflection Data

HYP_BEGIN_CLASS(DescriptorTableBase, 88, 1, NAME("ObjectBase"), ClassAttribute("abstract", true),ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion DescriptorTableBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorDeclaration Reflection Data

HYP_BEGIN_STRUCT(DescriptorDeclaration, 286, 0, {})
    Field(NAME(HYP_STR(Slot)), &DescriptorDeclaration::slot, offsetof(DescriptorDeclaration, slot), Span<const ClassAttribute> { {ClassAttribute("property", "Slot"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Name)), &DescriptorDeclaration::name, offsetof(DescriptorDeclaration, name), Span<const ClassAttribute> { {ClassAttribute("property", "Name"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Count)), &DescriptorDeclaration::count, offsetof(DescriptorDeclaration, count), Span<const ClassAttribute> { {ClassAttribute("property", "Count"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Size)), &DescriptorDeclaration::size, offsetof(DescriptorDeclaration, size), Span<const ClassAttribute> { {ClassAttribute("property", "Size"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(IsDynamic)), &DescriptorDeclaration::isDynamic, offsetof(DescriptorDeclaration, isDynamic), Span<const ClassAttribute> { {ClassAttribute("property", "IsDynamic"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Index)), &DescriptorDeclaration::index, offsetof(DescriptorDeclaration, index), Span<const ClassAttribute> { {ClassAttribute("property", "Index"), ClassAttribute("transient", true), ClassAttribute("serialize", false) } })
HYP_END_STRUCT

#pragma endregion DescriptorDeclaration Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetDeclaration Reflection Data

HYP_BEGIN_STRUCT(DescriptorSetDeclaration, 287, 0, {})
    Field(NAME(HYP_STR(SetIndex)), &DescriptorSetDeclaration::setIndex, offsetof(DescriptorSetDeclaration, setIndex), Span<const ClassAttribute> { {ClassAttribute("property", "SetIndex"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Name)), &DescriptorSetDeclaration::name, offsetof(DescriptorSetDeclaration, name), Span<const ClassAttribute> { {ClassAttribute("property", "Name"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Slots)), &DescriptorSetDeclaration::slots, offsetof(DescriptorSetDeclaration, slots), Span<const ClassAttribute> { {ClassAttribute("property", "Slots"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Flags)), &DescriptorSetDeclaration::flags, offsetof(DescriptorSetDeclaration, flags), Span<const ClassAttribute> { {ClassAttribute("property", "Flags"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion DescriptorSetDeclaration Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetBase Reflection Data

HYP_BEGIN_CLASS(DescriptorSetBase, 90, 1, NAME("ObjectBase"), ClassAttribute("abstract", true),ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion DescriptorSetBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetDeclarationFlags Reflection Data

HYP_BEGIN_ENUM(DescriptorSetDeclarationFlags, 288, 0, {})
    StaticField(NAME(HYP_STR(NONE)), DescriptorSetDeclarationFlags::NONE),
    StaticField(NAME(HYP_STR(REFERENCE)), DescriptorSetDeclarationFlags::REFERENCE),
    StaticField(NAME(HYP_STR(TEMPLATE)), DescriptorSetDeclarationFlags::TEMPLATE)
HYP_END_ENUM

#pragma endregion DescriptorSetDeclarationFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSlot Reflection Data

HYP_BEGIN_ENUM(DescriptorSlot, 289, 0, {})
    StaticField(NAME(HYP_STR(DESCRIPTOR_SLOT_NONE)), DescriptorSlot::DESCRIPTOR_SLOT_NONE),
    StaticField(NAME(HYP_STR(DESCRIPTOR_SLOT_SRV)), DescriptorSlot::DESCRIPTOR_SLOT_SRV),
    StaticField(NAME(HYP_STR(DESCRIPTOR_SLOT_UAV)), DescriptorSlot::DESCRIPTOR_SLOT_UAV),
    StaticField(NAME(HYP_STR(DESCRIPTOR_SLOT_CBUFF)), DescriptorSlot::DESCRIPTOR_SLOT_CBUFF),
    StaticField(NAME(HYP_STR(DESCRIPTOR_SLOT_SSBO)), DescriptorSlot::DESCRIPTOR_SLOT_SSBO),
    StaticField(NAME(HYP_STR(DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE)), DescriptorSlot::DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE),
    StaticField(NAME(HYP_STR(DESCRIPTOR_SLOT_SAMPLER)), DescriptorSlot::DESCRIPTOR_SLOT_SAMPLER),
    StaticField(NAME(HYP_STR(DESCRIPTOR_SLOT_MAX)), DescriptorSlot::DESCRIPTOR_SLOT_MAX)
HYP_END_ENUM

#pragma endregion DescriptorSlot Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DescriptorSetLayoutElement Reflection Data

HYP_BEGIN_STRUCT(DescriptorSetLayoutElement, 290, 0, {})
    Field(NAME(HYP_STR(Type)), &DescriptorSetLayoutElement::type, offsetof(DescriptorSetLayoutElement, type)),
    Field(NAME(HYP_STR(Binding)), &DescriptorSetLayoutElement::binding, offsetof(DescriptorSetLayoutElement, binding)),
    Field(NAME(HYP_STR(Count)), &DescriptorSetLayoutElement::count, offsetof(DescriptorSetLayoutElement, count)),
    Field(NAME(HYP_STR(Size)), &DescriptorSetLayoutElement::size, offsetof(DescriptorSetLayoutElement, size))
HYP_END_STRUCT

#pragma endregion DescriptorSetLayoutElement Reflection Data

} // namespace hyperion

