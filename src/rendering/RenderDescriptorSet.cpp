/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderImageView.hpp>
#include <rendering/RenderSampler.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/shader_compiler/ShaderCompiler.hpp>
#include <rendering/rt/RenderAccelerationStructure.hpp>

#include <rendering/Buffers.hpp>

namespace hyperion {

#pragma region DescriptorSetLayout

DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetDeclaration* decl)
    : decl(decl),
      isTemplate(false),
      isReference(false)
{
    if (!decl)
    {
        return;
    }

    name = decl->name;

    isTemplate = decl->flags[DescriptorSetDeclarationFlags::TEMPLATE];
    isReference = decl->flags[DescriptorSetDeclarationFlags::REFERENCE];

    if (isReference)
    {
        decl = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(decl->name);

        HYP_GFX_ASSERT(decl != nullptr, "Invalid global descriptor set reference: %s", decl->name.LookupString());

        this->decl = decl;
    }

    for (const Array<DescriptorDeclaration>& slot : decl->slots)
    {
        for (const DescriptorDeclaration& descriptor : slot)
        {
            const uint32 descriptorIndex = decl->CalculateFlatIndex(descriptor.slot, descriptor.name);
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
    for (const auto& it : elements)
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

    dynamicElements.Resize(dynamicElementsWithIndex.Size());

    for (SizeType i = 0; i < dynamicElementsWithIndex.Size(); i++)
    {
        dynamicElements[i] = dynamicElementsWithIndex[i].first;
    }
}

HashCode DescriptorSetLayout::GetHashCode() const
{
    HashCode hc;

    if (!decl)
    {
        return hc; // empty hash
    }

    hc.Add(decl->GetHashCode());

    for (const auto& it : elements)
    {
        hc.Add(it.first.GetHashCode());
        hc.Add(it.second.GetHashCode());
    }

    return hc;
}

#pragma endregion DescriptorSetLayout

#pragma region DescriptorSetElement

DescriptorSetElement::~DescriptorSetElement()
{
    if (values.Empty())
    {
        return;
    }

    for (auto& it : values)
    {
        if (!it.second.HasValue())
        {
            continue;
        }

        Visit(std::move(it.second), [](auto&& ref)
            {
                SafeRelease(std::move(ref));
            });
    }
}

#pragma endregion DescriptorSetElement

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
                    SafeRelease(std::move(ref));
                });
        }
    }
}

bool DescriptorSetBase::HasElement(Name name) const
{
    return m_elements.Find(name) != m_elements.End();
}

void DescriptorSetBase::SetElement(Name name, uint32 index, const GpuBufferRef& ref)
{
    SetElement<GpuBufferRef>(name, index, ref);
}

void DescriptorSetBase::SetElement(Name name, uint32 index, uint32 bufferSize, const GpuBufferRef& ref)
{
    SetElement<GpuBufferRef>(name, index, ref);
}

void DescriptorSetBase::SetElement(Name name, const GpuBufferRef& ref)
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

#pragma region DescriptorTableBase

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

const DescriptorSetRef& DescriptorTableBase::GetDescriptorSet(Name name, uint32 frameIndex) const
{
    for (const DescriptorSetRef& set : m_sets[frameIndex])
    {
        if (!set->GetLayout().IsValid())
        {
            continue;
        }

        if (set->GetLayout().GetDeclaration()->name == name)
        {
            return set;
        }
    }

    return DescriptorSetRef::unset;
}

const DescriptorSetRef& DescriptorTableBase::GetDescriptorSet(uint32 descriptorSetIndex, uint32 frameIndex) const
{
    for (const DescriptorSetRef& set : m_sets[frameIndex])
    {
        if (!set->GetLayout().IsValid())
        {
            continue;
        }

        if (set->GetLayout().GetDeclaration()->setIndex == descriptorSetIndex)
        {
            return set;
        }
    }

    return DescriptorSetRef::unset;
}

uint32 DescriptorTableBase::GetDescriptorSetIndex(Name name) const
{
    return m_decl ? m_decl->GetDescriptorSetIndex(name) : ~0u;
}

RendererResult DescriptorTableBase::Create()
{
    if (!IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Descriptor table declaration is not valid");
    }

    RendererResult result;

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        for (const DescriptorSetRef& set : m_sets[frameIndex])
        {
            Assert(set->GetLayout().IsValid());

            const Name descriptorSetName = set->GetLayout().GetDeclaration()->name;

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

RendererResult DescriptorTableBase::Destroy()
{
    for (auto& it : m_sets)
    {
        SafeRelease(std::move(it));
    }

    m_sets = {};

    return {};
}

#pragma endregion DescriptorTableBase

} // namespace hyperion