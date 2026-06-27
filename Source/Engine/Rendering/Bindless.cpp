/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Bindless.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>

namespace Hyperion {

BindlessStorage::BindlessStorage() = default;
BindlessStorage::~BindlessStorage() = default;

void BindlessStorage::UnsetAllResources(BindlessStorageSlot slot)
{
    AssertOnThread(g_renderThread);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = RI.globalDescriptorTable->GetDescriptorSet(BindlessStorageSlotNames[slot], frameIndex);
        AssertDebug(descriptorSet.IsValid());

        // Unset all active textures
        for (auto it = m_resources[slot].Begin(); it != m_resources[slot].End(); ++it)
        {
            const uint32 index = m_resources[slot].IndexOf(it);

            descriptorSet->DeleteElement(BindlessStorageDescriptorNames[slot], index);
        }

        descriptorSet->Update(true);
    }

    m_resources[slot].Clear();
    m_idGenerators[slot].Reset();
}

void BindlessStorage::AddResource(BindlessStorageSlot slot, uint32 index, const Handle<ObjectBase>& resource)
{
    AssertOnThread(g_renderThread);

    Assert(index < MaxBindlessResources[slot]);
    Assert(resource.IsValid());

    auto& resources = m_resources[slot];

    if (resources.HasIndex(index) && resources.Get(index).GetUnsafe() == resource.Get())
    {
        // same value
        return;
    }

    resources.Emplace(index, MakeWeakRef(resource));

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = RI.globalDescriptorTable->GetDescriptorSet(BindlessStorageSlotNames[slot], frameIndex);
        AssertDebug(descriptorSet.IsValid());

        static constexpr auto TypeIdGpuImageView = CONSTEXPR_TYPE_ID(GpuImageView);
        static constexpr auto TypeIdGpuBuffer = CONSTEXPR_TYPE_ID(GpuBuffer);

        switch (resource->InstanceClass()->GetTypeId().Value())
        {
        case TypeIdGpuImageView:
            descriptorSet->SetElement(BindlessStorageDescriptorNames[slot], index, static_cast<GpuImageView*>(resource.Get()));
            break;
        case TypeIdGpuBuffer:
            descriptorSet->SetElement(BindlessStorageDescriptorNames[slot], index, static_cast<GpuBuffer*>(resource.Get()));
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
}

void BindlessStorage::RemoveResource(BindlessStorageSlot slot, uint32 index)
{
    AssertOnThread(g_renderThread);

    auto& resources = m_resources[slot];

    if (!resources.HasIndex(index))
    {
        return;
    }

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = RI.globalDescriptorTable->GetDescriptorSet(BindlessStorageSlotNames[slot], frameIndex);
        AssertDebug(descriptorSet.IsValid());

        if (slot == BindlessStorage_Textures)
        {
            GpuImageView* placeholderView = RI.placeholderData->GetImageView2D1x1R8();
            descriptorSet->SetElement(BindlessStorageDescriptorNames[slot], index, placeholderView);
        }
        else
        {
            descriptorSet->DeleteElement(BindlessStorageDescriptorNames[slot], index);
        }
    }

    resources.EraseAt(index);
}

} // namespace Hyperion
