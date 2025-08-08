/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowMap.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/Texture.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region ShadowMapAtlas

bool ShadowMapAtlas::AddElement(const Vec2u& elementDimensions, ShadowMapAtlasElement& outElement)
{
    if (!AtlasPacker<ShadowMapAtlasElement>::AddElement(elementDimensions, outElement))
    {
        return false;
    }

    outElement.layerIndex = atlasIndex;

    return true;
}

#pragma endregion ShadowMapAtlas

#pragma region ShadowMapAllocator

ShadowMapAllocator::ShadowMapAllocator()
    : m_atlasDimensions(2048, 2048)
{
    m_atlases.Reserve(4);

    for (SizeType i = 0; i < 4; i++)
    {
        m_atlases.PushBack(ShadowMapAtlas(uint32(i), m_atlasDimensions));
    }
}

ShadowMapAllocator::~ShadowMapAllocator()
{
    SafeRelease(std::move(m_atlasImage));
    SafeRelease(std::move(m_atlasImageView));

    SafeRelease(std::move(m_pointLightShadowMapImage));
    SafeRelease(std::move(m_pointLightShadowMapImageView));
}

void ShadowMapAllocator::Initialize()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    m_atlasImage = g_renderBackend->MakeImage(TextureDesc {
        TT_TEX2D_ARRAY,
        TF_RG16F,
        Vec3u { m_atlasDimensions, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        uint32(m_atlases.Size()),
        IU_SAMPLED | IU_STORAGE });

    HYP_GFX_ASSERT(m_atlasImage->Create());

    m_atlasImageView = g_renderBackend->MakeImageView(m_atlasImage);
    HYP_GFX_ASSERT(m_atlasImageView->Create());

    m_pointLightShadowMapImage = g_renderBackend->MakeImage(TextureDesc {
        TT_CUBEMAP_ARRAY,
        TF_R16,
        Vec3u { 256, 256, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        g_maxBoundPointShadowMaps * 6,
        IU_SAMPLED | IU_STORAGE });

    HYP_GFX_ASSERT(m_pointLightShadowMapImage->Create());

    m_pointLightShadowMapImageView = g_renderBackend->MakeImageView(m_pointLightShadowMapImage);
    HYP_GFX_ASSERT(m_pointLightShadowMapImageView->Create());
}

void ShadowMapAllocator::Destroy()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    for (ShadowMapAtlas& atlas : m_atlases)
    {
        atlas.Clear();
    }

    SafeRelease(std::move(m_atlasImage));
    SafeRelease(std::move(m_atlasImageView));

    SafeRelease(std::move(m_pointLightShadowMapImage));
    SafeRelease(std::move(m_pointLightShadowMapImageView));
}

ShadowMap* ShadowMapAllocator::AllocateShadowMap(ShadowMapType shadowMapType, ShadowMapFilter filterMode, const Vec2u& dimensions)
{
    if (shadowMapType == SMT_OMNI)
    {
        const uint32 pointLightIndex = m_pointLightShadowMapIdGenerator.Next() - 1;

        // Cannot allocate if we ran out of IDs
        if (pointLightIndex >= g_maxBoundPointShadowMaps)
        {
            m_pointLightShadowMapIdGenerator.ReleaseId(pointLightIndex + 1);

            return nullptr;
        }

        const ShadowMapAtlasElement atlasElement {
            .layerIndex = pointLightIndex,
            .offsetUv = Vec2f::Zero(),
            .offsetCoords = Vec2u::Zero(),
            .dimensions = dimensions,
            .scale = Vec2f::One()
        };

        ShadowMap* shadowMap = new ShadowMap(
            shadowMapType,
            filterMode,
            atlasElement,
            m_pointLightShadowMapImageView);

        return shadowMap;
    }

    for (ShadowMapAtlas& atlas : m_atlases)
    {
        ShadowMapAtlasElement atlasElement;

        if (atlas.AddElement(dimensions, atlasElement))
        {
            GpuImageViewRef atlasImageView = m_atlasImage->MakeLayerImageView(atlasElement.layerIndex);
            DeferCreate(atlasImageView);

            ShadowMap* shadowMap = new ShadowMap(
                shadowMapType,
                filterMode,
                atlasElement,
                atlasImageView);

            return shadowMap;
        }
    }

    return nullptr;
}

bool ShadowMapAllocator::FreeShadowMap(ShadowMap* shadowMap)
{
    if (!shadowMap)
    {
        return false;
    }

    Assert(shadowMap->GetAtlasElement() != nullptr);
    const ShadowMapAtlasElement& atlasElement = *shadowMap->GetAtlasElement();

    bool result = false;

    if (atlasElement.layerIndex != ~0u)
    {
        if (shadowMap->GetShadowMapType() == SMT_OMNI)
        {
            m_pointLightShadowMapIdGenerator.ReleaseId(atlasElement.layerIndex + 1);
            
            result = true;
        }
        else
        {
            Assert(atlasElement.layerIndex < m_atlases.Size());
            
            ShadowMapAtlas& atlas = m_atlases[atlasElement.layerIndex];
            result = atlas.RemoveElement(atlasElement);
            
            if (!result)
            {
                HYP_LOG(Rendering, Error, "Failed to free shadow map from atlas (atlas index: {})", atlasElement.layerIndex);
            }
        }
    }
    else
    {
        HYP_LOG(Rendering, Error, "Failed to free shadow map: invalid layer index");
    }

    delete shadowMap;

    return result;
}

#pragma endregion ShadowMapAllocator

} // namespace hyperion
