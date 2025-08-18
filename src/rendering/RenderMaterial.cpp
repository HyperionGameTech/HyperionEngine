/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderConfig.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <core/Types.hpp>

namespace hyperion {

#pragma region MaterialDescriptorSetManager

MaterialDescriptorSetManager::MaterialDescriptorSetManager()
{
}

MaterialDescriptorSetManager::~MaterialDescriptorSetManager()
{
    SafeDelete(std::move(m_fallbackMaterialDescriptorSets));

    for (auto& it : m_materialDescriptorSets)
    {
        SafeDelete(std::move(it.second));
    }

    m_materialDescriptorSets.Clear();
}

void MaterialDescriptorSetManager::CreateFallbackMaterialDescriptorSet()
{
    if (g_renderBackend->GetRenderConfig().bindlessTextures)
    {
        return;
    }

    const DescriptorSetDeclaration* decl = g_renderGlobalState->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration("Material");
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        m_fallbackMaterialDescriptorSets[frameIndex] = g_renderBackend->MakeDescriptorSet(layout);
        m_fallbackMaterialDescriptorSets[frameIndex]->SetDebugName(NAME_FMT("MaterialDescriptorSet_INVALID_{}", frameIndex));

        for (uint32 textureIndex = 0; textureIndex < g_maxBoundTextures; textureIndex++)
        {
            m_fallbackMaterialDescriptorSets[frameIndex]->SetElement("Textures", textureIndex, g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultTexture2d));
        }

        DeferCreate(m_fallbackMaterialDescriptorSets[frameIndex]);
    }

    m_materialDescriptorSets.Set(~0u, m_fallbackMaterialDescriptorSets);
}

const DescriptorSetRef& MaterialDescriptorSetManager::ForBoundMaterial(const Material* material, uint32 frameIndex)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    uint32 boundIndex = ~0u;

    if (material)
    {
        boundIndex = RenderApi_RetrieveResourceBinding(material);

        AssertDebug(boundIndex != ~0u, "Material {} is not bound for rendering!", material->Id());
    }

    if (boundIndex != ~0u)
    {
        const auto it = m_materialDescriptorSets.Find(boundIndex);

        if (it != m_materialDescriptorSets.End() && it->second[frameIndex].IsValid())
        {

            return it->second[frameIndex];
        }
    }

    AssertDebug(m_fallbackMaterialDescriptorSets[frameIndex].IsValid()
        && m_fallbackMaterialDescriptorSets[frameIndex]->IsCreated());

    return m_fallbackMaterialDescriptorSets[frameIndex];
}

FixedArray<DescriptorSetRef, g_framesInFlight> MaterialDescriptorSetManager::Allocate(uint32 boundIndex)
{
    if (boundIndex == ~0u)
    {
        return {};
    }

    Threads::AssertOnThread(g_renderThread);

    const DescriptorSetDeclaration* decl = g_renderGlobalState->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration("Material");
    Assert(decl != nullptr);

    DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, g_framesInFlight> descriptorSets;

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);

#ifdef HYP_DEBUG_MODE
        descriptorSet->SetDebugName(NAME_FMT("MaterialDescriptorSet_{}_{}", boundIndex, frameIndex));
#endif

        for (uint32 textureIndex = 0; textureIndex < g_maxBoundTextures; textureIndex++)
        {
            descriptorSet->SetElement("Textures", textureIndex, g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultTexture2d));
        }

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        if (RendererResult res = descriptorSets[frameIndex]->Create(); res.HasError())
        {
            HYP_FAIL("Failed to create descriptor set! {}", res.GetError().GetMessage());
        }
    }

    auto it = m_materialDescriptorSets.Find(boundIndex);
    if (it != m_materialDescriptorSets.End())
    {
        SafeDelete(std::move(it->second));
    }

    m_materialDescriptorSets[boundIndex] = descriptorSets;

    return descriptorSets;
}

FixedArray<DescriptorSetRef, g_framesInFlight> MaterialDescriptorSetManager::Allocate(
    uint32 boundIndex,
    Span<const uint32> textureIndirectIndices,
    Span<const Handle<Texture>> textures)
{
    if (boundIndex == ~0u)
    {
        return {};
    }

    Threads::AssertOnThread(g_renderThread);

    const DescriptorSetDeclaration* decl = g_renderGlobalState->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration("Material");
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, g_framesInFlight> descriptorSets;

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);

#ifdef HYP_DEBUG_MODE
        descriptorSet->SetDebugName(NAME_FMT("MaterialDescriptorSet_{}_{}", boundIndex, frameIndex));
#endif

        // set initial placeholder elements that will get overridden
        for (uint32 i = 0; i < g_maxBoundTextures; i++)
        {
            descriptorSet->SetElement("Textures", i, g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultTexture2d));
        }

        for (uint32 slot = 0; slot < uint32(textureIndirectIndices.Size()); slot++)
        {
            const uint32 textureIndex = textureIndirectIndices[slot];

            if (textureIndex == ~0u)
            {
                continue;
            }

            AssertDebug(textureIndex < textures.Size(),
                "Texture index %u is out of bounds of textures array size %llu",
                textureIndex, textures.Size());

            const Handle<Texture>& texture = textures[textureIndex];

            if (texture.IsValid())
            {
                descriptorSet->SetElement("Textures", textureIndex, g_renderBackend->GetTextureImageView(texture));
            }
        }

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        if (RendererResult res = descriptorSets[frameIndex]->Create(); res.HasError())
        {
            HYP_FAIL("Failed to create descriptor set! {}", res.GetError().GetMessage());
        }
    }

    auto it = m_materialDescriptorSets.Find(boundIndex);
    if (it != m_materialDescriptorSets.End())
    {
        SafeDelete(std::move(it->second));
    }

    m_materialDescriptorSets[boundIndex] = descriptorSets;

    return descriptorSets;
}

void MaterialDescriptorSetManager::Remove(uint32 boundIndex)
{
    Threads::AssertOnThread(g_renderThread);

    if (boundIndex == ~0u)
    {
        return;
    }

    const auto it = m_materialDescriptorSets.Find(boundIndex);

    if (it != m_materialDescriptorSets.End())
    {
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            SafeDelete(std::move(it->second[frameIndex]));
        }

        m_materialDescriptorSets.Erase(it);
    }
}

#pragma endregion MaterialDescriptorSetManager

} // namespace hyperion
