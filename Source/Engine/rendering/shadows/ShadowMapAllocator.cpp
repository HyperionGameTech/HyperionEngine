/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowMap.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Texture.hpp>

#include <rendering/CommandRecorder.hpp>

#include <rendering/TextureViewCache.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <ShadowMapAllocator.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region ShadowMapAtlas

bool ShadowMapAtlas::AddElement(const Vec2u& elementDimensions, ShadowMapAtlasElement*& outElement)
{
    outElement = nullptr;

    uint32 elementIndex = ~0u;

    if (!AtlasPacker<ShadowMapAtlasElement>::AddElement(elementDimensions, outElement, elementIndex))
    {
        HYP_LOG(Rendering, Warning, "Failed to add shadow map atlas element with dimensions {}x{}", elementDimensions.x, elementDimensions.y);
        return false;
    }

    AssertDebug(outElement != nullptr);

    outElement->index = elementIndex;
    outElement->layerIndex = atlasIndex;

    return true;
}

#pragma endregion ShadowMapAtlas

#pragma region ShadowMapAllocator

ShadowMapAllocator::ShadowMapAllocator()
    : m_atlasDimensions(2048, 2048)
{
    m_atlases.Reserve(4);

    for (size_t i = 0; i < 4; i++)
    {
        m_atlases.PushBack(ShadowMapAtlas(uint32(i), m_atlasDimensions));
    }
}

ShadowMapAllocator::~ShadowMapAllocator()
{
    EnqueueDeletion(std::move(m_atlasTextureArray));
    EnqueueDeletion(std::move(m_pointLightTextureArray));
    EnqueueDeletion(std::move(m_clearTexture));
}

void ShadowMapAllocator::Initialize()
{
    m_atlasTextureArray = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2DArray,
        TextureFormat::D16,
        Vec3u { m_atlasDimensions, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        uint16(m_atlases.Size()),
        IU_SAMPLED | IU_ATTACHMENT
    });

    m_atlasTextureArray->SetName(NAME("ShadowMapAtlas"));
    CheckResult(m_atlasTextureArray->Create());
    m_atlasTextureArray->GetGpuImage()->SetDebugName(NAME("ShadowMapAtlas"));

    m_pointLightTextureArray = MakeHandle<Texture>(TextureDesc {
        TextureType::CubemapArray,
        TextureFormat::D16,
        Vec3u { 256, 256, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        MaxBoundOmniShadowMaps * 6,
        IU_SAMPLED | IU_ATTACHMENT
    });

    m_pointLightTextureArray->SetName(NAME("PointLightShadowMapImage"));
    CheckResult(m_pointLightTextureArray->Create());
    m_pointLightTextureArray->GetGpuImage()->SetDebugName(NAME("PointLightShadowMapImage"));

    m_clearTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2DArray,
        TextureFormat::D16,
        Vec3u { m_atlasDimensions, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_ATTACHMENT
    });
    m_clearTexture->SetName(NAME("ShadowMapClearTexture"));
    CheckResult(m_clearTexture->Create());
    m_clearTexture->GetGpuImage()->SetDebugName(NAME("ShadowMapClearTexture"));

    { // Clear that clear texture
        CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

        cr << InsertBarrier(m_clearTexture->GetGpuImage(), RS_COPY_DST);
        cr << FillImage(m_clearTexture->GetGpuImage(), 1.0f, ImageSubResource {});
        cr << InsertBarrier(m_clearTexture->GetGpuImage(), RS_COPY_SRC);

        cr.Done();
    }
}

void ShadowMapAllocator::Shutdown()
{
    for (ShadowMapAtlas& atlas : m_atlases)
    {
        atlas.Clear();
    }

    EnqueueDeletion(std::move(m_atlasTextureArray));
    EnqueueDeletion(std::move(m_pointLightTextureArray));
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

            HYP_LOG_ONCE(Rendering, Warning, "Too many omni shadow maps allocated; returning NULL for AllocateShadowMap()");

            return nullptr;
        }

        ShadowMapAtlasElement atlasElement {};
        atlasElement.layerIndex = pointLightIndex;
        atlasElement.offsetUV = Vec2f::Zero();
        atlasElement.offsetCoords = Vec2u::Zero();
        atlasElement.dimensions = dimensions;
        atlasElement.scale = Vec2f::One();

        ImageSubResource subResource {};
        subResource.baseArrayLayer = atlasElement.layerIndex * 6;
        subResource.numLayers = 6;
        subResource.baseMipLevel = 0;
        subResource.numLevels = 1;

        // @NOTE using TextureType::Texture2DArray for point light shadow maps rather than CubemapArray
        GpuImageViewRef atlasImageView = RI.textureViewCache->GetOrCreate(
            m_pointLightTextureArray,
            subResource,
            TextureType::Texture2DArray);

        CheckResult(atlasImageView->Create());

        ShadowMap* shadowMap = new ShadowMap(
            shadowMapType,
            filterMode,
            atlasElement,
            atlasImageView);

        return shadowMap;
    }

    for (ShadowMapAtlas& atlas : m_atlases)
    {
        ShadowMapAtlasElement* atlasElement = nullptr;

        if (atlas.AddElement(dimensions, atlasElement))
        {
            AssertDebug(atlasElement != nullptr);

            ImageSubResource subResource {};
            subResource.baseArrayLayer = atlasElement->layerIndex;
            subResource.numLayers = 1;
            subResource.baseMipLevel = 0;
            subResource.numLevels = 1;

            GpuImageViewRef atlasImageView = RI.textureViewCache->GetOrCreate(m_atlasTextureArray, subResource);
            CheckResult(atlasImageView->Create());

            ShadowMap* shadowMap = new ShadowMap(
                shadowMapType,
                filterMode,
                *atlasElement,
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
            else
            {
                Frame* frame = RI.GetCurrentFrame();
                if (frame != nullptr)
                {
                    ImageSubResource srcSubResource {};
                    srcSubResource.baseArrayLayer = 0;
                    srcSubResource.numLayers = 1;
                    srcSubResource.baseMipLevel = 0;
                    srcSubResource.numLevels = 1;

                    ImageSubResource dstSubResource {};
                    dstSubResource.baseArrayLayer = atlasElement.layerIndex;
                    dstSubResource.numLayers = 1;
                    dstSubResource.baseMipLevel = 0;
                    dstSubResource.numLevels = 1;

                    Vec3u srcOffset = Vec3u(atlasElement.offsetCoords, 0);
                    Vec3u extent = Vec3u(atlasElement.dimensions, 1);

                    frame->cr << InsertBarrier(
                        m_atlasTextureArray->GetGpuImage(),
                        RS_COPY_DST,
                        dstSubResource);

                    frame->cr << CopyImage(
                        m_clearTexture->GetGpuImage(),
                        m_atlasTextureArray->GetGpuImage(),
                        srcOffset,
                        Vec3u::Zero(),
                        extent,
                        srcSubResource,
                        dstSubResource);
                }
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
