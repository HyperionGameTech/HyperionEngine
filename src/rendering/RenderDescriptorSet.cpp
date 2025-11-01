/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderConfig.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/Buffers.hpp>

namespace hyperion {
#pragma region DescriptorSetDeclaration

DescriptorDeclaration* DescriptorSetDeclaration::FindDescriptorDeclaration(WeakName name) const
{
    for (uint32 slotIndex = 0; slotIndex < DESCRIPTOR_SLOT_MAX; slotIndex++)
    {
        for (const DescriptorDeclaration& decl : slots[slotIndex])
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
    HYP_GFX_ASSERT(slot != DESCRIPTOR_SLOT_NONE && slot < DESCRIPTOR_SLOT_MAX);

    uint32 flatIndex = 0;

    for (uint32 slotIndex = 0; slotIndex < uint32(slot); slotIndex++)
    {
        if (slotIndex == uint32(slot) - 1)
        {
            uint32 declIndex = 0;

            for (const DescriptorDeclaration& decl : slots[slotIndex])
            {
                if (decl.name == name)
                {
                    return flatIndex + declIndex;
                }

                declIndex++;
            }
        }

        flatIndex += slots[slotIndex].Size();
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

DescriptorSetDeclaration* DescriptorTableDeclaration::AddDescriptorSetDeclaration(DescriptorSetDeclaration&& descriptorSetDeclaration)
{
    return &elements.PushBack(std::move(descriptorSetDeclaration));
}

DescriptorTableDeclaration& GetStaticDescriptorTableDeclaration()
{
    static struct Initializer
    {
        DescriptorTableDeclaration decl;

        DescriptorTableDeclaration::DeclareSet globalSet { &decl, 0, NAME("Global") };
        DescriptorTableDeclaration::DeclareSet viewSet { &decl, 1, NAME("View"), /* isTemplate */ true };
        DescriptorTableDeclaration::DeclareSet objectSet { &decl, 2, NAME("Object") };
        DescriptorTableDeclaration::DeclareSet materialSet { &decl, 3, NAME("Material") };
    } initializer;

    return initializer.decl;
}

#pragma endregion DescriptorSetDeclaration

#pragma region DescriptorSetLayout

DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetDeclaration* decl)
    : m_decl(decl),
      m_isTemplate(false),
      m_isReference(false)
{
    if (!decl)
    {
        return;
    }

    m_isTemplate = decl->flags[DescriptorSetDeclarationFlags::TEMPLATE];
    m_isReference = decl->flags[DescriptorSetDeclarationFlags::REFERENCE];

    if (m_isReference)
    {
        m_decl = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(decl->name);

        HYP_GFX_ASSERT(m_decl != nullptr, "Invalid global descriptor set reference: %s", decl->name.LookupString());
    }

    for (const Array<DescriptorDeclaration>& slot : m_decl->slots)
    {
        for (const DescriptorDeclaration& descriptor : slot)
        {
            const uint32 descriptorIndex = m_decl->CalculateFlatIndex(descriptor.slot, descriptor.name);
            HYP_GFX_ASSERT(descriptorIndex != ~0u);

            if (descriptor.cond != nullptr && !descriptor.cond())
            {
                // Skip this descriptor, condition not met
                continue;
            }

            // HYP_LOG(RenderingBackend, Debug, "Set element {}.{}[{}] (slot: {}, count: {}, size: {}, is_dynamic: {})",
            //     declPtr->name, descriptor.name, descriptorIndex, int(descriptor.slot),
            //     descriptor.count, descriptor.size, descriptor.isDynamic);

            switch (descriptor.slot)
            {
            case DescriptorSlot::DESCRIPTOR_SLOT_SRV:
                AddElement(descriptor.name, DescriptorSetElementType::IMAGE, descriptorIndex, descriptor.count);

                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_UAV:
                AddElement(descriptor.name, DescriptorSetElementType::IMAGE_STORAGE, descriptorIndex, descriptor.count);

                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_CBUFF:
                if (descriptor.isDynamic)
                {
                    AddElement(descriptor.name, DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC, descriptorIndex, descriptor.count, descriptor.size);
                }
                else
                {
                    AddElement(descriptor.name, DescriptorSetElementType::UNIFORM_BUFFER, descriptorIndex, descriptor.count, descriptor.size);
                }
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_SSBO:
                if (descriptor.isDynamic)
                {
                    AddElement(descriptor.name, DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC, descriptorIndex, descriptor.count, descriptor.size);
                }
                else
                {
                    AddElement(descriptor.name, DescriptorSetElementType::SSBO, descriptorIndex, descriptor.count, descriptor.size);
                }
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE:
                AddElement(descriptor.name, DescriptorSetElementType::TLAS, descriptorIndex, descriptor.count);

                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_SAMPLER:
                AddElement(descriptor.name, DescriptorSetElementType::SAMPLER, descriptorIndex, descriptor.count);

                break;
            default:
                HYP_UNREACHABLE();
            }
        }
    }

    // build a list of dynamic elements, paired by their element index so we can sort it after.
    Array<Pair<Name, uint32>> dynamicElementsWithIndex;

    // Add to list of dynamic buffer names
    for (const auto& it : m_elements)
    {
        if (it.second.type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC
            || it.second.type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC)
        {
            dynamicElementsWithIndex.PushBack({ it.first, it.second.binding });
        }
    }

    std::sort(dynamicElementsWithIndex.Begin(), dynamicElementsWithIndex.End(), [](const Pair<Name, uint32>& a, const Pair<Name, uint32>& b)
        {
            return a.second < b.second;
        });

    m_dynamicElements.Resize(dynamicElementsWithIndex.Size());

    for (SizeType i = 0; i < dynamicElementsWithIndex.Size(); i++)
    {
        m_dynamicElements[i] = dynamicElementsWithIndex[i].first;
    }
}

#pragma endregion DescriptorSetLayout

#pragma region DescriptorSetBase

DescriptorSetBase::~DescriptorSetBase()
{
    for (auto& elementsIt : m_elements)
    {
        for (auto& valuesIt : elementsIt.second.values)
        {
            DescriptorSetElement::ValueType& value = valuesIt.second;

            if (!value.HasValue())
            {
                continue;
            }

            Visit(std::move(value), [](auto&& ref)
                {
                    SafeDelete(std::move(ref));
                });
        }
    }
}

bool DescriptorSetBase::HasElement(WeakName name) const
{
    return m_elements.FindAs(name) != m_elements.End();
}

void DescriptorSetBase::SetElement(WeakName name, uint32 index, const GpuBufferRef& ref)
{
    SetElementT<GpuBufferBase>(name, index, ref);
}

void DescriptorSetBase::SetElement(WeakName name, uint32 index, uint32 bufferSize, const GpuBufferRef& ref)
{
    SetElementT<GpuBufferBase>(name, index, ref);
}

void DescriptorSetBase::SetElement(WeakName name, const GpuBufferRef& ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSetBase::SetElement(WeakName name, uint32 index, const GpuImageViewRef& ref)
{
    SetElementT<GpuImageViewBase>(name, index, ref);
}

void DescriptorSetBase::SetElement(WeakName name, const GpuImageViewRef& ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSetBase::SetElement(WeakName name, uint32 index, const SamplerRef& ref)
{
    SetElementT<SamplerBase>(name, index, ref);
}

void DescriptorSetBase::SetElement(WeakName name, const SamplerRef& ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSetBase::SetElement(WeakName name, uint32 index, const TLASRef& ref)
{
    SetElementT<TLASBase>(name, index, ref);
}

void DescriptorSetBase::SetElement(WeakName name, const TLASRef& ref)
{
    SetElement(name, 0, ref);
}

#pragma endregion DescriptorSetBase

} // namespace hyperion
