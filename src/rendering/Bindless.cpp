/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/Bindless.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderInterface.hpp>

#include <rendering/DescriptorSet.hpp>

#include <rendering/Texture.hpp>

namespace Hyperion {

BindlessStorage::BindlessStorage() = default;
BindlessStorage::~BindlessStorage()
{
}

void BindlessStorage::UnsetAllResources()
{
    AssertOnThread(g_renderThread);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet("Material", frameIndex);
        AssertDebug(descriptorSet.IsValid());

        // Unset all active textures
        for (const auto& it : m_resources)
        {
            descriptorSet->SetElement("Textures", it.first.ToIndex(), g_renderBackend->GetTextureImageView(g_renderInterface->placeholderData->defaultTexture2d));
        }
    }

    m_resources.Clear();
}

void BindlessStorage::AddResource(ObjId<Texture> id, const GpuImageViewRef& imageView)
{
    AssertOnThread(g_renderThread);

    if (!id.IsValid())
    {
        return;
    }

    auto it = m_resources.Find(id);

    if (it != m_resources.End())
    {
        return;
    }

    m_resources.Insert({ id, imageView });

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet("Material", frameIndex);
        AssertDebug(descriptorSet.IsValid());

        descriptorSet->SetElement("Textures", id.ToIndex(), imageView);
    }
}

void BindlessStorage::RemoveResource(ObjId<Texture> id)
{
    AssertOnThread(g_renderThread);

    if (!id.IsValid())
    {
        return;
    }

    auto it = m_resources.Find(id);

    if (it == m_resources.End())
    {
        return;
    }

    m_resources.Erase(it);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderInterface->globalDescriptorTable->GetDescriptorSet("Material", frameIndex);
        AssertDebug(descriptorSet.IsValid());

        descriptorSet->SetElement("Textures", id.ToIndex(), g_renderBackend->GetTextureImageView(g_renderInterface->placeholderData->defaultTexture2d));
    }
}

} // namespace Hyperion
