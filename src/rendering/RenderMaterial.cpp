/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <engine/EngineDriver.hpp>

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

    const DescriptorSetDeclaration* decl = g_renderInterface->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration("Material");
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_fallbackMaterialDescriptorSets[frameIndex] = g_renderBackend->MakeDescriptorSet(layout);
        m_fallbackMaterialDescriptorSets[frameIndex]->SetDebugName(NAME_FMT("MaterialDescriptorSet_INVALID_{}", frameIndex));

        // set dummy placeholder textures for each material
        for (Name textureName : Material::s_textureNames)
        {
            m_fallbackMaterialDescriptorSets[frameIndex]->SetElement(textureName, g_renderBackend->GetTextureImageView(g_renderInterface->placeholderData->defaultTexture2d));
        }

        DeferCreate(m_fallbackMaterialDescriptorSets[frameIndex]);
    }

    m_materialDescriptorSets.Set(~0u, m_fallbackMaterialDescriptorSets);
}

const DescriptorSetRef& MaterialDescriptorSetManager::ForBoundMaterial(const Material* material, uint32 frameIndex)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    uint32 boundIndex = ~0u;

    if (material)
    {
        boundIndex = RenderApi::RetrieveResourceBinding(material);

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

FixedArray<DescriptorSetRef, NumFramesInFlight> MaterialDescriptorSetManager::Allocate(uint32 boundIndex)
{
    if (boundIndex == ~0u)
    {
        return {};
    }

    AssertOnThread(g_renderThread);

    const DescriptorSetDeclaration* decl = g_renderInterface->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration("Material");
    Assert(decl != nullptr);

    DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, NumFramesInFlight> descriptorSets;

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);

#ifdef HYP_DEBUG_MODE
        descriptorSet->SetDebugName(NAME_FMT("MaterialDescriptorSet_{}_{}", boundIndex, frameIndex));
#endif

        // set dummy placeholder textures for each material
        for (Name textureName : Material::s_textureNames)
        {
            descriptorSet->SetElement(textureName, g_renderBackend->GetTextureImageView(g_renderInterface->placeholderData->defaultTexture2d));
        }

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
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

FixedArray<DescriptorSetRef, NumFramesInFlight> MaterialDescriptorSetManager::Allocate(
    uint32 boundIndex,
    Span<const uint32> textureIndirectIndices,
    Span<const Handle<Texture>> textures)
{
    if (boundIndex == ~0u)
    {
        return {};
    }

    AssertOnThread(g_renderThread);

    const DescriptorSetDeclaration* decl = g_renderInterface->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration("Material");
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, NumFramesInFlight> descriptorSets;

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);

#ifdef HYP_DEBUG_MODE
        descriptorSet->SetDebugName(NAME_FMT("MaterialDescriptorSet_{}_{}", boundIndex, frameIndex));
#endif

        for (uint32 slot = 0; slot < uint32(textureIndirectIndices.Size()); slot++)
        {
            if (slot >= Material::s_textureNames.Size())
            {
                break;
            }

            Name textureName = Material::s_textureNames[slot];

            const uint32 textureIndex = textureIndirectIndices[slot];

            if (textureIndex != ~0u)
            {
                AssertDebug(textureIndex < textures.Size(),
                    "Texture index %u is out of bounds of textures array size %llu",
                    textureIndex, textures.Size());

                const Handle<Texture>& texture = textures[textureIndex];

                if (texture != nullptr)
                {
                    descriptorSet->SetElement(textureName, g_renderBackend->GetTextureImageView(texture));

                    continue;
                }
            }

            // set placeholder texture
            descriptorSet->SetElement(textureName, g_renderBackend->GetTextureImageView(g_renderInterface->placeholderData->defaultTexture2d));
        }

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
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
    AssertOnThread(g_renderThread);

    if (boundIndex == ~0u)
    {
        return;
    }

    const auto it = m_materialDescriptorSets.Find(boundIndex);

    if (it != m_materialDescriptorSets.End())
    {
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            SafeDelete(std::move(it->second[frameIndex]));
        }

        m_materialDescriptorSets.Erase(it);
    }
}

#pragma endregion MaterialDescriptorSetManager

} // namespace hyperion
