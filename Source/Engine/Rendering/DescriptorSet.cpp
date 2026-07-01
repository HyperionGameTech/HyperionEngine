/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Sampler.hpp>

#include <Rendering/AccelerationStructure.hpp>

#include <DescriptorSet.generated.inl>

namespace Hyperion {

static constexpr uint32 ElementTypeToBufferType[uint32(ShaderInputType::MAX)] = {
    0,                                             // Unset
    (1u << uint32(GpuBufferType::ConstantBuffer)), // CBV
    (1u << uint32(GpuBufferType::ConstantBuffer)), // CBV_Dynamic
    (1u << uint32(GpuBufferType::StructuredBuffer))
        | (1u << uint32(GpuBufferType::RWStructuredBuffer))
        | (1u << uint32(GpuBufferType::ByteAddressBuffer))
        | (1u << uint32(GpuBufferType::RWByteAddressBuffer))
        | (1u << uint32(GpuBufferType::ReadbackBuffer))
        | (1u << uint32(GpuBufferType::StagingBuffer))
        | (1u << uint32(GpuBufferType::IndirectArgsBuffer))
        | (1u << uint32(GpuBufferType::RTMeshIndexBuffer))
        | (1u << uint32(GpuBufferType::RTMeshVertexBuffer)), // SRV
    (1u << uint32(GpuBufferType::StructuredBuffer))
        | (1u << uint32(GpuBufferType::RWStructuredBuffer))
        | (1u << uint32(GpuBufferType::ByteAddressBuffer))
        | (1u << uint32(GpuBufferType::RWByteAddressBuffer))
        | (1u << uint32(GpuBufferType::ReadbackBuffer))
        | (1u << uint32(GpuBufferType::StagingBuffer))
        | (1u << uint32(GpuBufferType::IndirectArgsBuffer))
        | (1u << uint32(GpuBufferType::RTMeshIndexBuffer))
        | (1u << uint32(GpuBufferType::RTMeshVertexBuffer)), // SRV_Dynamic
    (1u << uint32(GpuBufferType::RWStructuredBuffer))
        | (1u << uint32(GpuBufferType::RWByteAddressBuffer))
        | (1u << uint32(GpuBufferType::IndirectArgsBuffer)), // UAV
    (1u << uint32(GpuBufferType::RWStructuredBuffer))
        | (1u << uint32(GpuBufferType::RWByteAddressBuffer)), // UAV_Dynamic
    0                                                         // Sampler
};

#pragma region ShaderInputSet

ShaderInput* ShaderInputSet::FindDescriptorDeclaration(StringHash name) const
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

uint32 ShaderInputSet::CalculateFlatIndex(ShaderRegister slot, StringHash name) const
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

void ShaderInputSet::RecalculateIndices()
{
    for (auto& slot : slots)
    {
        for (uint32 i = 0; i < slot.Size(); i++)
        {
            slot[i].index = i;
        }
    }
}

ShaderInputSet* ShaderInputGroup::FindDescriptorSetDeclaration(StringHash name) const
{
    for (const ShaderInputSet& decl : elements)
    {
        if (decl.name == name)
        {
            return const_cast<ShaderInputSet*>(&decl);
        }
    }

    return nullptr;
}

ShaderInputSet* ShaderInputGroup::AddDescriptorSetDeclaration(ShaderInputSet&& inputSet)
{
    return &elements.PushBack(std::move(inputSet));
}

void ShaderInputGroup::RecalculateAllIndices()
{
    for (auto& element : elements)
    {
        if (element.setIndex != ~0u)
        {
            element.RecalculateIndices();
        }
    }
}

ShaderInputGroup& GetStaticDescriptorTableDeclaration()
{
    static ShaderInputGroup s_decl;
    static ShaderInputGroup::DeclareSet s_BindlessResources0Decl { &s_decl, 0, NAME("BindlessResources0") };
    static ShaderInputGroup::DeclareSet s_BindlessResources1Decl { &s_decl, 1, NAME("BindlessResources1") };

    return s_decl;
}

#pragma endregion ShaderInputSet

#pragma region DescriptorSetLayout

DescriptorSetLayout::DescriptorSetLayout(const ShaderInputSet* decl)
    : m_decl(decl),
      m_isTemplate(false),
      m_isReference(false),
      m_cachedHashCode(HashCode())
{
    if (!decl)
    {
        return;
    }

    m_isTemplate = decl->flags[ShaderInputSetFlags::Template];
    m_isReference = decl->flags[ShaderInputSetFlags::Reference];

    if (m_isReference)
    {
        m_decl = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(decl->name);

        Assert(m_decl != nullptr, "Invalid global descriptor set reference: {}", decl->name);
    }

    // build a list of dynamic elements, paired by their element index so we can sort it after.
    Array<Pair<Name, uint32>> dynamicElementsWithIndex;

    for (const Array<ShaderInput>& slot : m_decl->slots)
    {
        for (const ShaderInput& shaderInput : slot)
        {
            // #ifdef HYP_VULKAN
            const uint32 binding = m_decl->CalculateFlatIndex(shaderInput.slot, shaderInput.name);
            // #elif HYP_DX12
            //             const uint32 binding = shaderInput.index;
            // #endif

            Assert(binding != ~0u);

            if (shaderInput.cond != nullptr && !shaderInput.cond())
            {
                // Skip this descriptor, condition not met
                continue;
            }

            // HYP_LOG(RenderingBackend, Verbose, "Set element {}.{}[{}] (slot: {}, count: {}, size: {}, is_dynamic: {})",
            //     declPtr->name, descriptor.name, binding, int(descriptor.slot),
            //     descriptor.count, descriptor.size, descriptor.isDynamic);

            ShaderInputWithBinding& res = AddElement(shaderInput);
            res.binding = binding;

            if (shaderInput.type == ShaderInputType::CBV_Dynamic
                || shaderInput.type == ShaderInputType::SRV_Dynamic
                || shaderInput.type == ShaderInputType::UAV_Dynamic)
            {
                dynamicElementsWithIndex.PushBack({ shaderInput.name, binding });
            }
        }
    }

    std::sort(dynamicElementsWithIndex.Begin(), dynamicElementsWithIndex.End(), [](const Pair<Name, uint32>& a, const Pair<Name, uint32>& b)
              {
                  return a.second < b.second;
              });

    m_dynamicElements.Resize(dynamicElementsWithIndex.Size());

    for (size_t i = 0; i < dynamicElementsWithIndex.Size(); i++)
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
    const ShaderInput* shaderInput = m_layout.GetElement(name);
    AssertDebug(shaderInput != nullptr, "Invalid element: No item with name {} found", Name(name));

    AssertDebug(ref != nullptr);

    // Range check
    AssertDebug(index < shaderInput->count, "Index {} out of range for element {} with count {}",
                index, Name(name), shaderInput->count);

    if constexpr (std::is_base_of_v<GpuBufferBase, T>)
    {
        static constexpr uint32 Mask = (1u << uint32(ShaderInputType::CBV))
            | (1u << uint32(ShaderInputType::CBV_Dynamic))
            | (1u << uint32(ShaderInputType::SRV))
            | (1u << uint32(ShaderInputType::SRV_Dynamic))
            | (1u << uint32(ShaderInputType::UAV))
            | (1u << uint32(ShaderInputType::UAV_Dynamic));

        AssertDebug(Mask & (1u << uint32(shaderInput->type)), "Layout type for {} does not match given type", Name(name));

        const bool isByteAddressBuffer = (shaderInput->bufferType == GpuBufferType::ByteAddressBuffer
                                          || shaderInput->bufferType == GpuBufferType::RWByteAddressBuffer);

        if (bufferStride == ByteAddressBufferStride)
        {
            AssertDebug(isByteAddressBuffer);
        }

        if (ref != nullptr)
        {
            // Buffer type check, to make sure the buffer type is allowed for the given element
            const GpuBufferType bufferType = ref->GetBufferType();

            AssertDebug(
                (ElementTypeToBufferType[uint32(shaderInput->type)] & (1u << uint32(bufferType))),
                "Buffer type {} is not in the allowed types for element {}",
                EnumToString(bufferType), Name(name));
        }
    }
    else if constexpr (std::is_base_of_v<GpuImageViewBase, T>)
    {
        static constexpr uint32 Mask = (1u << uint32(ShaderInputType::SRV))
            | (1u << uint32(ShaderInputType::UAV));

        AssertDebug(Mask & (1u << uint32(shaderInput->type)), "Layout type for {} does not match given type", Name(name));
    }
    else if constexpr (std::is_base_of_v<SamplerBase, T>)
    {
        static constexpr uint32 Mask = (1u << uint32(ShaderInputType::Sampler));

        AssertDebug(Mask & (1u << uint32(shaderInput->type)), "Layout type for {} does not match given type", Name(name));
    }
    else if constexpr (std::is_base_of_v<TopLevelASBase, T>)
    {
        static constexpr uint32 Mask = (1u << uint32(ShaderInputType::SRV));

        AssertDebug(Mask & (1u << uint32(shaderInput->type)), "Layout type for {} does not match given type", Name(name));
    }
    else
    {
        static_assert(always_fail_v<T>, "Unsupported type for descriptor set element");
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
            // if (element->values[index] == ref)
            //{
            //     // same object reference; skip marking dirty; unless TLAS (the tlas can change)
            //     return *element;
            // }

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

void DescriptorSetBase::SetElement(StringHash name, uint32 index, TopLevelAS* ref)
{
    SetElementT<TopLevelAS>(name, index, ref);
}

void DescriptorSetBase::SetElement(StringHash name, TopLevelAS* ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSetBase::DeleteElement(StringHash name, uint32 index)
{
    const ShaderInputWithBinding* layoutElement = m_layout.GetElement(name);
    Assert(layoutElement != nullptr);

    // ~0u count == bindless
    Assert(layoutElement->count == ~0u, "Can only call DeleteElement() for bindless descriptors");

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

    for (const ShaderInputSet& inputSet : m_decl->elements)
    {
        if (inputSet.flags[ShaderInputSetFlags::Reference])
        {
            const ShaderInputSet* referencedDescriptorSetDeclaration = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(inputSet.name);
            Assert(referencedDescriptorSetDeclaration != nullptr, "Invalid global descriptor set reference: {}", inputSet.name);

            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                DescriptorSetRef descriptorSet = RI.globalDescriptorTable->GetDescriptorSet(referencedDescriptorSetDeclaration->name, frameIndex);
                Assert(descriptorSet.IsValid(), "Invalid global descriptor set reference: {}", referencedDescriptorSetDeclaration->name);

                m_sets[frameIndex].PushBack(std::move(descriptorSet));
            }

            continue;
        }

        DescriptorSetLayout layout { &inputSet };

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            DescriptorSetRef descriptorSet = MakeHandle<DescriptorSet>(layout);

#ifdef HYP_RHI_DEBUG_NAMES
            descriptorSet->SetDebugName(layout.GetName());
#endif

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
            ShaderInputSet* decl = m_decl->FindDescriptorSetDeclaration(descriptorSetName);
            AssertDebug(decl != nullptr);

            if ((decl->flags & ShaderInputSetFlags::Reference))
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
