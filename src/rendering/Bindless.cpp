/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/Bindless.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/RenderDescriptorSet.hpp>

#include <rendering/Texture.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

BindlessStorage::BindlessStorage() = default;
BindlessStorage::~BindlessStorage()
{
}

void BindlessStorage::UnsetAllResources()
{
    Threads::AssertOnThread(g_renderThread);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Material", frameIndex);
        AssertDebug(descriptorSet.IsValid());

        // Unset all active textures
        for (const auto& it : m_textures)
        {
            descriptorSet->SetElement("Textures", it.first, g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultTexture2d));
        }
    }

    m_textures.Clear();
}

void BindlessStorage::AddResource(uint32 boundIndex, Texture* texture)
{
    Threads::AssertOnThread(g_renderThread);

    if (boundIndex == ~0u || !texture)
    {
        return;
    }

    auto it = m_textures.Find(boundIndex);

    if (it != m_textures.End())
    {
        return;
    }

    m_textures.Insert({ boundIndex, MakeWeakRef(texture) });

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Material", frameIndex);
        AssertDebug(descriptorSet.IsValid());

        descriptorSet->SetElement("Textures", boundIndex, g_renderBackend->GetTextureImageView(texture));
    }
}

void BindlessStorage::RemoveResource(uint32 boundIndex)
{
    Threads::AssertOnThread(g_renderThread);

    if (boundIndex == ~0u)
    {
        return;
    }

    auto it = m_textures.Find(boundIndex);

    if (it == m_textures.End())
    {
        return;
    }

    m_textures.Erase(it);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Material", frameIndex);
        AssertDebug(descriptorSet.IsValid());

        descriptorSet->SetElement("Textures", boundIndex, g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultTexture2d));
    }
}

} // namespace hyperion
