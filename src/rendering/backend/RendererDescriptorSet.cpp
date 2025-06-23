/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <rendering/Buffers.hpp>

namespace hyperion {
#pragma region DescriptorSetDeclaration

DescriptorDeclaration* DescriptorSetDeclaration::FindDescriptorDeclaration(WeakName name) const
{
    for (uint32 slot_index = 0; slot_index < DESCRIPTOR_SLOT_MAX; slot_index++)
    {
        for (const DescriptorDeclaration& decl : slots[slot_index])
        {
            if (decl.name == name)
            {
                return const_cast<DescriptorDeclaration*>(&decl);
            }
        }
    }

    return nullptr;
}

uint32 DescriptorSetDeclaration::CalculateFlatIndex(DescriptorSlot slot, WeakName name) const
{
    AssertThrow(slot != DESCRIPTOR_SLOT_NONE && slot < DESCRIPTOR_SLOT_MAX);

    uint32 flat_index = 0;

    for (uint32 slot_index = 0; slot_index < uint32(slot); slot_index++)
    {
        if (slot_index == uint32(slot) - 1)
        {
            uint32 decl_index = 0;

            for (const DescriptorDeclaration& decl : slots[slot_index])
            {
                if (decl.name == name)
                {
                    return flat_index + decl_index;
                }

                decl_index++;
            }
        }

        flat_index += slots[slot_index].Size();
    }

    return ~0u;
}

DescriptorSetDeclaration* DescriptorTableDeclaration::FindDescriptorSetDeclaration(WeakName name) const
{
    for (const DescriptorSetDeclaration& decl : elements)
    {
        if (decl.name == name)
        {
            return const_cast<DescriptorSetDeclaration*>(&decl);
        }
    }

    return nullptr;
}

DescriptorSetDeclaration* DescriptorTableDeclaration::AddDescriptorSetDeclaration(DescriptorSetDeclaration&& descriptor_set_declaration)
{
    return &elements.PushBack(std::move(descriptor_set_declaration));
}

DescriptorTableDeclaration& GetStaticDescriptorTableDeclaration()
{
    static struct Initializer
    {
        DescriptorTableDeclaration decl;

        DescriptorTableDeclaration::DeclareSet global_set { &decl, 0, NAME("Global") };
        DescriptorTableDeclaration::DeclareSet view_set { &decl, 1, NAME("View"), /* is_template */ true };
        DescriptorTableDeclaration::DeclareSet object_set { &decl, 2, NAME("Object") };
        DescriptorTableDeclaration::DeclareSet material_set { &decl, 3, NAME("Material"), /* is_template */ true };
    } initializer;

    return initializer.decl;
}

#pragma endregion DescriptorSetDeclaration

#pragma region DescriptorSetLayout

DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetDeclaration* decl)
    : m_decl(decl),
      m_is_template(false),
      m_is_reference(false)
{
    if (!decl)
    {
        return;
    }

    m_is_template = decl->flags[DescriptorSetDeclarationFlags::TEMPLATE];
    m_is_reference = decl->flags[DescriptorSetDeclarationFlags::REFERENCE];

    if (m_is_reference)
    {
        m_decl = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(decl->name);

        AssertThrowMsg(m_decl != nullptr, "Invalid global descriptor set reference: %s", decl->name.LookupString());
    }

    for (const Array<DescriptorDeclaration>& slot : m_decl->slots)
    {
        for (const DescriptorDeclaration& descriptor : slot)
        {
            const uint32 descriptor_index = m_decl->CalculateFlatIndex(descriptor.slot, descriptor.name);
            AssertThrow(descriptor_index != ~0u);

            if (descriptor.cond != nullptr && !descriptor.cond())
            {
                // Skip this descriptor, condition not met
                continue;
            }

            // HYP_LOG(RenderingBackend, Debug, "Set element {}.{}[{}] (slot: {}, count: {}, size: {}, is_dynamic: {})",
            //     decl_ptr->name, descriptor.name, descriptor_index, int(descriptor.slot),
            //     descriptor.count, descriptor.size, descriptor.is_dynamic);

            switch (descriptor.slot)
            {
            case DescriptorSlot::DESCRIPTOR_SLOT_SRV:
                AddElement(descriptor.name, DescriptorSetElementType::IMAGE, descriptor_index, descriptor.count);

                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_UAV:
                AddElement(descriptor.name, DescriptorSetElementType::IMAGE_STORAGE, descriptor_index, descriptor.count);

                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_CBUFF:
                if (descriptor.is_dynamic)
                {
                    AddElement(descriptor.name, DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC, descriptor_index, descriptor.count, descriptor.size);
                }
                else
                {
                    AddElement(descriptor.name, DescriptorSetElementType::UNIFORM_BUFFER, descriptor_index, descriptor.count, descriptor.size);
                }
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_SSBO:
                if (descriptor.is_dynamic)
                {
                    AddElement(descriptor.name, DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC, descriptor_index, descriptor.count, descriptor.size);
                }
                else
                {
                    AddElement(descriptor.name, DescriptorSetElementType::STORAGE_BUFFER, descriptor_index, descriptor.count, descriptor.size);
                }
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE:
                AddElement(descriptor.name, DescriptorSetElementType::TLAS, descriptor_index, descriptor.count);

                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_SAMPLER:
                AddElement(descriptor.name, DescriptorSetElementType::SAMPLER, descriptor_index, descriptor.count);

                break;
            default:
                AssertThrowMsg(false, "Invalid descriptor slot");
                break;
            }
        }
    }

    // build a list of dynamic elements, paired by their element index so we can sort it after.
    Array<Pair<Name, uint32>> dynamic_elements_with_index;

    // Add to list of dynamic buffer names
    for (const auto& it : m_elements)
    {
        if (it.second.type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC
            || it.second.type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC)
        {
            dynamic_elements_with_index.PushBack({ it.first, it.second.binding });
        }
    }

    std::sort(dynamic_elements_with_index.Begin(), dynamic_elements_with_index.End(), [](const Pair<Name, uint32>& a, const Pair<Name, uint32>& b)
        {
            return a.second < b.second;
        });

    m_dynamic_elements.Resize(dynamic_elements_with_index.Size());

    for (SizeType i = 0; i < dynamic_elements_with_index.Size(); i++)
    {
        m_dynamic_elements[i] = dynamic_elements_with_index[i].first;
    }
}

#pragma endregion DescriptorSetLayout

#pragma region DescriptorSetBase

DescriptorSetBase::~DescriptorSetBase()
{
    for (auto& elements_it : m_elements)
    {
        for (auto& values_it : elements_it.second.values)
        {
            DescriptorSetElement::ValueType& value = values_it.second;

            if (!value.HasValue())
            {
                continue;
            }

            Visit(std::move(value), [](auto&& ref)
                {
                    SafeRelease(std::move(ref));
                });
        }
    }
}

bool DescriptorSetBase::HasElement(Name name) const
{
    return m_elements.Find(name) != m_elements.End();
}

void DescriptorSetBase::SetElement(Name name, uint32 index, const GPUBufferRef& ref)
{
    SetElement<GPUBufferRef>(name, index, ref);
}

void DescriptorSetBase::SetElement(Name name, uint32 index, uint32 buffer_size, const GPUBufferRef& ref)
{
    SetElement<GPUBufferRef>(name, index, ref);
}

void DescriptorSetBase::SetElement(Name name, const GPUBufferRef& ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSetBase::SetElement(Name name, uint32 index, const ImageViewRef& ref)
{
    SetElement<ImageViewRef>(name, index, ref);
}

void DescriptorSetBase::SetElement(Name name, const ImageViewRef& ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSetBase::SetElement(Name name, uint32 index, const SamplerRef& ref)
{
    SetElement<SamplerRef>(name, index, ref);
}

void DescriptorSetBase::SetElement(Name name, const SamplerRef& ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSetBase::SetElement(Name name, uint32 index, const TLASRef& ref)
{
    SetElement<TLASRef>(name, index, ref);
}

void DescriptorSetBase::SetElement(Name name, const TLASRef& ref)
{
    SetElement(name, 0, ref);
}

#pragma endregion DescriptorSetBase

} // namespace hyperion