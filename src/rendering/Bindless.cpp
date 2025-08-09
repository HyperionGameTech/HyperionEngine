/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Material", frameIndex);
        AssertDebug(descriptorSet.IsValid());

        // Unset all active textures
        for (const auto& it : m_resources)
        {
            descriptorSet->SetElement("Textures", it.first.ToIndex(), g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultTexture2d));
        }
    }

    m_resources.Clear();
}

void BindlessStorage::AddResource(ObjId<Texture> id, const GpuImageViewRef& imageView)
{
    Threads::AssertOnThread(g_renderThread);

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

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Material", frameIndex);
        AssertDebug(descriptorSet.IsValid());

        descriptorSet->SetElement("Textures", id.ToIndex(), imageView);
    }
}

void BindlessStorage::RemoveResource(ObjId<Texture> id)
{
    Threads::AssertOnThread(g_renderThread);

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

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Material", frameIndex);
        AssertDebug(descriptorSet.IsValid());

        descriptorSet->SetElement("Textures", id.ToIndex(), g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultTexture2d));
    }
}

} // namespace hyperion
