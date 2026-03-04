/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowMap.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Texture.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <ShadowMapAllocator.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region ShadowMapAtlas

bool ShadowMapAtlas::AddElement(const Vec2u& elementDimensions, ShadowMapAtlasElement& outElement)
{
    uint32 elementIndex = ~0u;

    if (!AtlasPacker<ShadowMapAtlasElement>::AddElement(elementDimensions, outElement, elementIndex))
    {
        return false;
    }

    outElement.index = elementIndex;
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
    EnqueueDeletion(std::move(m_atlasImage));
    EnqueueDeletion(std::move(m_atlasImageView));

    EnqueueDeletion(std::move(m_pointLightShadowMapImage));
    EnqueueDeletion(std::move(m_pointLightShadowMapImageView));
}

void ShadowMapAllocator::Initialize()
{
    m_atlasImage = g_renderInterface->MakeImage(TextureDesc {
        TextureType::Texture2DArray,
        TextureFormat::D16,
        Vec3u { m_atlasDimensions, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        uint16(m_atlases.Size()),
        IU_SAMPLED | IU_ATTACHMENT
    });
    
#if HYP_DEBUG_MODE
    m_atlasImage->SetDebugName(NAME("ShadowMapAtlasImage"));
#endif

    CheckResult(m_atlasImage->Create());

    m_atlasImageView = g_renderInterface->MakeImageView(m_atlasImage);
#if HYP_DEBUG_MODE
    m_atlasImageView->SetDebugName(NAME("ShadowMapAtlasImageView"));
#endif

    CheckResult(m_atlasImageView->Create());

    m_pointLightShadowMapImage = g_renderInterface->MakeImage(TextureDesc {
        TextureType::CubemapArray,
        TextureFormat::RG16F, // Variance shadow maps are used for point lights
        Vec3u { 256, 256, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        MaxBoundOmniShadowMaps * 6,
        IU_SAMPLED | IU_ATTACHMENT
    });
    
#if HYP_DEBUG_MODE
    m_pointLightShadowMapImage->SetDebugName(NAME("PointLightShadowMapImage"));
#endif

    CheckResult(m_pointLightShadowMapImage->Create());

    m_pointLightShadowMapImageView = g_renderInterface->MakeImageView(m_pointLightShadowMapImage);
    
#if HYP_DEBUG_MODE
    m_pointLightShadowMapImageView->SetDebugName(NAME("PointLightShadowMapImageView"));
#endif

    CheckResult(m_pointLightShadowMapImageView->Create());
}

void ShadowMapAllocator::Shutdown()
{
    for (ShadowMapAtlas& atlas : m_atlases)
    {
        atlas.Clear();
    }

    EnqueueDeletion(std::move(m_atlasImage));
    EnqueueDeletion(std::move(m_atlasImageView));

    EnqueueDeletion(std::move(m_pointLightShadowMapImage));
    EnqueueDeletion(std::move(m_pointLightShadowMapImageView));
}

ShadowMap* ShadowMapAllocator::AllocateShadowMap(ShadowMapType shadowMapType, ShadowMapFilter filterMode, const Vec2u& dimensions)
{
    HYP_SCOPE;

    AssertDebug(dimensions.Volume() > 0);

    if (shadowMapType == SMT_OMNI)
    {
        const uint32 pointLightIndex = m_pointLightShadowMapIdGenerator.Next() - 1;

        // Cannot allocate if we ran out of IDs
        if (pointLightIndex >= MaxBoundOmniShadowMaps)
        {
            m_pointLightShadowMapIdGenerator.ReleaseId(pointLightIndex + 1);

            HYP_LOG(Rendering, Error, "Too mani omni shadow maps allocated");
            
            return nullptr;
        }

        const ShadowMapAtlasElement atlasElement {
            .layerIndex = pointLightIndex,
            .offsetUV = Vec2f::Zero(),
            .offsetCoords = Vec2u::Zero(),
            .dimensions = dimensions,
            .scale = Vec2f::One()
        };

        ImageSubResource subResource {};
        subResource.baseArrayLayer = atlasElement.layerIndex * 6;
        subResource.numLayers = 6;
        subResource.baseMipLevel = 0;
        subResource.numLevels = 1;
        
        GpuImageViewRef atlasImageView = MakeHandle<GpuImageView>(m_pointLightShadowMapImage, subResource);
        DeferCreate(atlasImageView);

        ShadowMap* shadowMap = new ShadowMap(
            shadowMapType,
            filterMode,
            atlasElement,
            atlasImageView);

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

    HYP_LOG(Rendering, Error, "Shadow map could not be fit into an atlas, dimensions = {}, num atlases = {}", dimensions, m_atlases.Size());

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
                HYP_LOG(Rendering, Error, "Failed to remove shadow map from atlas - not found! (atlas index: {})", atlasElement.layerIndex);
            }
        }
    }
    else
    {
        HYP_LOG(Rendering, Error, "Failed to remove shadow map from atlas - invalid layer index");
    }

    // delete even if not removed from atlas, caller is expecting it
    delete shadowMap;

    return result;
}

#pragma endregion ShadowMapAllocator

} // namespace Hyperion
