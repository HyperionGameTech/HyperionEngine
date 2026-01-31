/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/GpuImageView.hpp>
#include <rendering/Sampler.hpp>

#include <rendering/AccelerationStructure.hpp>

#include <DescriptorSet.generated.inl>

namespace Hyperion {
#pragma region DescriptorSetDeclaration

ShaderInput* DescriptorSetDeclaration::FindDescriptorDeclaration(StringHash name) const
{
    for (uint32 slotIndex = 0; slotIndex < uint8(ShaderRegister::MAX); slotIndex++)
    {
        for (const ShaderInput& decl : slots[slotIndex])
        {
            if (decl.name == name)
            {
                return const_cast<ShaderInput*>(&decl);
            }
        }
    }

    return nullptr;
}

uint32 DescriptorSetDeclaration::CalculateFlatIndex(ShaderRegister slot, StringHash name) const
{
    Assert(slot != ShaderRegister::NONE && uint8(slot) < uint8(ShaderRegister::MAX));

    uint32 flatIndex = 0;

    for (uint8 slotIndex = 0; slotIndex < uint8(slot); slotIndex++)
    {
        if (slotIndex == uint8(slot) - 1)
        {
            uint32 declIndex = 0;

            for (const ShaderInput& decl : slots[slotIndex])
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

DescriptorSetDeclaration* ShaderInputGroup::FindDescriptorSetDeclaration(StringHash name) const
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

DescriptorSetDeclaration* ShaderInputGroup::AddDescriptorSetDeclaration(DescriptorSetDeclaration&& descriptorSetDeclaration)
{
    return &elements.PushBack(std::move(descriptorSetDeclaration));
}

ShaderInputGroup& GetStaticDescriptorTableDeclaration()
{
    static ShaderInputGroup s_decl;
    static ShaderInputGroup::DeclareSet s_BindlessResources0Decl { &s_decl, 0, NAME("BindlessResources0") };
    static ShaderInputGroup::DeclareSet s_BindlessResources1Decl { &s_decl, 1, NAME("BindlessResources1") };

    return s_decl;
}

#pragma endregion DescriptorSetDeclaration

#pragma region DescriptorSetLayout

DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetDeclaration* decl)
    : m_decl(decl),
      m_isTemplate(false),
      m_isReference(false),
      m_cachedHashCode(HashCode())
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

        Assert(m_decl != nullptr, "Invalid global descriptor set reference: {}", decl->name);
    }

    for (const Array<ShaderInput>& slot : m_decl->slots)
    {
        for (const ShaderInput& descriptor : slot)
        {
            const uint32 descriptorIndex = m_decl->CalculateFlatIndex(descriptor.slot, descriptor.name);
            Assert(descriptorIndex != ~0u);

            if (descriptor.cond != nullptr && !descriptor.cond())
            {
                // Skip this descriptor, condition not met
                continue;
            }

            // HYP_LOG(RenderingBackend, Debug, "Set element {}.{}[{}] (slot: {}, count: {}, size: {}, is_dynamic: {})",
            //     declPtr->name, descriptor.name, descriptorIndex, int(descriptor.slot),
            //     descriptor.count, descriptor.size, descriptor.isDynamic);

            AddElement(descriptor.name, descriptor.type, descriptorIndex, descriptor.count);
        }
    }

    // build a list of dynamic elements, paired by their element index so we can sort it after.
    Array<Pair<Name, uint32>> dynamicElementsWithIndex;

    // Add to list of dynamic buffer names
    for (const auto& it : m_elements)
    {
        if (it.second.type == ShaderInputType::UNIFORM_BUFFER_DYNAMIC
            || it.second.type == ShaderInputType::STORAGE_BUFFER_DYNAMIC)
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

    m_cachedHashCode = m_elements.GetHashCode();
}

#pragma endregion DescriptorSetLayout

#pragma region DescriptorSetBase

DescriptorSetBase::~DescriptorSetBase() = default;

bool DescriptorSetBase::HasElement(StringHash name) const
{
    return m_elements.FindAs(name) != m_elements.End();
}

template <class T>
DescriptorSetElement& DescriptorSetBase::SetElementT(StringHash name, uint32 index, T* ref, uint32 bufferStride)
{
    const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(name);
    AssertDebug(layoutElement != nullptr, "Invalid element: No item with name {} found", Name(name));

    AssertDebug(ref != nullptr);

    // Range check
    AssertDebug(index < layoutElement->count, "Index {} out of range for element {} with count {}",
        index, Name(name), layoutElement->count);

    if constexpr (std::is_base_of_v<GpuBufferBase, T>)
    {
        static constexpr uint32 Mask = (1u << uint32(ShaderInputType::UNIFORM_BUFFER))
            | (1u << uint32(ShaderInputType::UNIFORM_BUFFER_DYNAMIC))
            | (1u << uint32(ShaderInputType::STORAGE_BUFFER))
            | (1u << uint32(ShaderInputType::STORAGE_BUFFER_DYNAMIC));

        AssertDebug(Mask & (1u << uint32(layoutElement->type)), "Layout type for {} does not match given type", Name(name));

        if (ref != nullptr)
        {
            // Buffer type check, to make sure the buffer type is allowed for the given element
            const GpuBufferType bufferType = ref->GetBufferType();

            AssertDebug(
                (ElementTypeToBufferType[uint32(layoutElement->type)] & (1u << uint32(bufferType))),
                "Buffer type {} is not in the allowed types for element {}",
                uint32(bufferType), Name(name));

            if (layoutElement->size != 0 && layoutElement->size != ~0u)
            {
                const uint32 remainder = ref->Size() % layoutElement->size;

                // AssertDebug(
                //     remainder == 0,
                //     "Buffer size ({}) is not a multiplier of layout size ({}) for element {}",
                //     ref->Size(), layoutElement->size, Name(name));
            }
        }
    }
    else if constexpr (std::is_base_of_v<GpuImageViewBase, T>)
    {
        static constexpr uint32 Mask = (1u << uint32(ShaderInputType::IMAGE))
            | (1u << uint32(ShaderInputType::IMAGE_STORAGE));

        AssertDebug(Mask & (1u << uint32(layoutElement->type)), "Layout type for {} does not match given type", Name(name));
    }
    else if constexpr (std::is_base_of_v<SamplerBase, T>)
    {
        static constexpr uint32 Mask = (1u << uint32(ShaderInputType::SAMPLER));

        AssertDebug(Mask & (1u << uint32(layoutElement->type)), "Layout type for {} does not match given type", Name(name));
    }
    else if constexpr (std::is_base_of_v<GpuTlasBase, T>)
    {
        static constexpr uint32 Mask = (1u << uint32(ShaderInputType::TLAS));

        AssertDebug(Mask & (1u << uint32(layoutElement->type)), "Layout type for {} does not match given type", Name(name));
    }
    else
    {
        static_assert(ResolutionFailureV<T>, "Unsupported type for descriptor set element");
    }

    DescriptorSetElement* element = nullptr;

    auto it = m_elements.Find(Name(name));

    if (it == m_elements.End())
    {
        it = m_elements.Emplace(Name(name)).first;
        
        element = &it->second;

        element->bufferStride = bufferStride;

        element->values.Resize(index + 1);
        element->values[index] = ref;

        element->occupiedArrayElems.Set(index, true);
    }
    else
    {
        element = &it->second;

        element->bufferStride = bufferStride;

        if (!element->occupiedArrayElems.Test(index))
        {
            element->values.Resize(MathUtil::Max(element->values.Size(), index + 1));
            element->values[index] = ref;

            element->occupiedArrayElems.Set(index, true);
        }
        else
        {
            //if (element->values[index] == ref)
            //{
            //    // same object reference; skip marking dirty; unless TLAS (the tlas can change)
            //    return *element;
            //}

            element->values[index] = ref;
        }
    }

    // Mark the range as dirty so that it will be updated in the next update
    element->dirtyRange |= { index, index + 1 };

    return *element;
}

void DescriptorSetBase::SetElement(StringHash name, uint32 index, GpuBuffer* ref, uint32 bufferStride)
{
    SetElementT<GpuBuffer>(name, index, ref, bufferStride);
}

void DescriptorSetBase::SetElement(StringHash name, uint32 index, uint32 bufferSize, GpuBuffer* ref, uint32 bufferStride)
{
    SetElementT<GpuBuffer>(name, index, ref, bufferStride);
}

void DescriptorSetBase::SetElement(StringHash name, GpuBuffer* ref, uint32 bufferStride)
{
    SetElement(name, 0, ref, bufferStride);
}

void DescriptorSetBase::SetElement(StringHash name, uint32 index, GpuImageView* ref)
{
    SetElementT<GpuImageView>(name, index, ref);
}

void DescriptorSetBase::SetElement(StringHash name, GpuImageView* ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSetBase::SetElement(StringHash name, uint32 index, Sampler* ref)
{
    SetElementT<Sampler>(name, index, ref);
}

void DescriptorSetBase::SetElement(StringHash name, Sampler* ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSetBase::SetElement(StringHash name, uint32 index, GpuTlas* ref)
{
    SetElementT<GpuTlas>(name, index, ref);
}

void DescriptorSetBase::SetElement(StringHash name, GpuTlas* ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSetBase::DeleteElement(StringHash name, uint32 index)
{
    const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(name);
    Assert(layoutElement != nullptr);
    Assert(layoutElement->IsBindless(), "Can only call DeleteElement() for bindless descriptors");

    DescriptorSetElement* element = nullptr;

    auto it = m_elements.Find(Name(name));

    if (it == m_elements.End())
    {
        return;
    }

    element = &it->second;

    if (!element->occupiedArrayElems.Test(index))
    {
        return;
    }

    element->values[index] = nullptr;
    element->occupiedArrayElems.Set(index, false);
}

#pragma endregion DescriptorSetBase

#pragma region DescriptorTableBase

DescriptorTableBase::DescriptorTableBase(const ShaderInputGroup* decl)
    : m_decl(decl)
{
    AssertDebug(decl != nullptr);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_sets[frameIndex].Reserve(m_decl->elements.Size());
    }

    for (const DescriptorSetDeclaration& descriptorSetDeclaration : m_decl->elements)
    {
        if (descriptorSetDeclaration.flags[DescriptorSetDeclarationFlags::REFERENCE])
        {
            const DescriptorSetDeclaration* referencedDescriptorSetDeclaration = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(descriptorSetDeclaration.name);
            Assert(referencedDescriptorSetDeclaration != nullptr, "Invalid global descriptor set reference: {}", descriptorSetDeclaration.name);

            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                DescriptorSetRef descriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet(referencedDescriptorSetDeclaration->name, frameIndex);
                Assert(descriptorSet.IsValid(), "Invalid global descriptor set reference: {}", referencedDescriptorSetDeclaration->name);

                m_sets[frameIndex].PushBack(std::move(descriptorSet));
            }

            continue;
        }

        DescriptorSetLayout layout { &descriptorSetDeclaration };

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            DescriptorSetRef descriptorSet = MakeHandle<DescriptorSet>(layout);
            descriptorSet->SetDebugName(layout.GetName());

            m_sets[frameIndex].PushBack(std::move(descriptorSet));
        }
    }
}

RendererResult DescriptorTableBase::Create()
{
    if (!IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Descriptor table declaration is not valid");
    }

    RendererResult result;

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        for (const DescriptorSetRef& set : m_sets[frameIndex])
        {
            const Name descriptorSetName = set->GetLayout().GetName();

            // use FindDescriptorSetDeclaration rather than `set->GetLayout().GetDeclaration()`, since we need to know
            // if the descriptor set is a reference to a global set
            DescriptorSetDeclaration* decl = m_decl->FindDescriptorSetDeclaration(descriptorSetName);
            AssertDebug(decl != nullptr);

            if ((decl->flags & DescriptorSetDeclarationFlags::REFERENCE))
            {
                // should be created elsewhere
                continue;
            }

            result = set->Create();

            if (!result)
            {
                return result;
            }
        }
    }

    return result;
}

void DescriptorTableBase::Update(uint32 frameIndex, bool force)
{
    if (!IsValid())
    {
        return;
    }

    for (const DescriptorSetRef& set : m_sets[frameIndex])
    {
        const DescriptorSetLayout& layout = set->GetLayout();

        if (layout.IsReference() || layout.IsTemplate())
        {
            // references are updated elsewhere
            // template descriptor sets are not updated (no handle to update)
            continue;
        }

        bool isDirty = false;
        set->UpdateDirtyState(&isDirty);

        if (!isDirty && !force)
        {
            continue;
        }

        set->Update(force);
    }
}

#pragma endregion DescriptorTableBase

} // namespace Hyperion
