/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/Bindless.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>

namespace Hyperion {

BindlessStorage::BindlessStorage() = default;
BindlessStorage::~BindlessStorage()
{
}

void BindlessStorage::UnsetAllResources(BindlessStorageSlot slot)
{
    AssertOnThread(g_renderThread);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet(BindlessStorageSlotNames[slot], frameIndex);
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
        const DescriptorSetRef& descriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet(BindlessStorageSlotNames[slot], frameIndex);
        AssertDebug(descriptorSet.IsValid());

        if (GpuImageView* imageView = DynamicCast<GpuImageView>(resource.Get()))
        {
            descriptorSet->SetElement(BindlessStorageDescriptorNames[slot], index, imageView);
        }
        else if (GpuBuffer* buffer = DynamicCast<GpuBuffer>(resource.Get()))
        {
            descriptorSet->SetElement(BindlessStorageDescriptorNames[slot], index, buffer);
        }
        else
        {
            HYP_FAIL("Invalid object type {} for bindless resources!", resource->InstanceClass()->GetName());
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
        const DescriptorSetRef& descriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet(BindlessStorageSlotNames[slot], frameIndex);
        AssertDebug(descriptorSet.IsValid());

        if (slot == BindlessStorage_Textures)
        {
            GpuImageView* placeholder_view = g_renderInterface->placeholderData->GetImageView2D1x1R8();
            descriptorSet->SetElement(BindlessStorageDescriptorNames[slot], index, placeholder_view);
        }
        else
        {
            descriptorSet->DeleteElement(BindlessStorageDescriptorNames[slot], index);
        }
    }

    resources.EraseAt(index);
}

} // namespace Hyperion
