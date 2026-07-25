/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Scene/LightmapVolume.hpp>
#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/LightmapElementComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Threads.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

#if HYP_EDITOR
#include <Baking/BakerSubsystem.hpp>
#include <Baking/LightmapVolume/LightmapVolumeBakeData.hpp>
#endif

#include <LightmapVolume.generated.inl>

namespace Hyperion {

#if HYP_EDITOR
EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
#endif // HYP_EDITOR

LightmapVolume::LightmapVolume()
    : LightmapVolume(BoundingBox::Empty())
{
}

LightmapVolume::LightmapVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds),
      m_irradianceAtlasTextures {},
      m_radianceAtlasTextures {}
{
    m_atlases.Reserve(MaxAtlasesPerLightmapVolume);
    m_atlases.EmplaceBack(DefaultAtlasDimensions);
}

LightmapVolume::~LightmapVolume()
{
    if (AnyOf(m_radianceAtlasTextures, &Handle<Texture>::IsValid))
    {
        EnqueueDeletion(std::move(m_radianceAtlasTextures));
    }

    if (AnyOf(m_irradianceAtlasTextures, &Handle<Texture>::IsValid))
    {
        EnqueueDeletion(std::move(m_irradianceAtlasTextures));
    }
}

bool LightmapVolume::AddElement(Vec2u dimensions, LightmapElement*& outElement, bool shrinkToFit, float downscaleLimit)
{
    outElement = nullptr;

    Optional<LightmapVolumeAtlas> tmpAtlas;

    for (uint32 atlasIndex = 0; atlasIndex < MaxAtlasesPerLightmapVolume; atlasIndex++)
    {
        LightmapVolumeAtlas* atlas = nullptr;
        bool isNewAtlas = false;

        if (atlasIndex >= m_atlases.Size())
        {
            atlas = &tmpAtlas.Emplace(DefaultAtlasDimensions);
            isNewAtlas = true;
        }
        else
        {
            atlas = &m_atlases[atlasIndex];
        }

        uint32 elementIndex = ~0u;

        if (atlas->AddElement(dimensions, outElement, elementIndex, shrinkToFit, downscaleLimit))
        {
            AssertDebug(elementIndex < UINT16_MAX);

            outElement->id = LightmapElementId(uint32((atlasIndex << 16) | elementIndex));

            if (isNewAtlas)
            {
                m_atlases.Resize(MathUtil::Max(m_atlases.Size(), atlasIndex + 1));

                m_atlases[atlasIndex] = std::move(*atlas);
            }

            SetNeedsRenderProxyUpdate();

            return true;
        }
    }

    // could not add to any atlas
    return false;
}

const LightmapElement* LightmapVolume::GetElement(LightmapElementId elementId) const
{
    uint16 atlasIndex;
    uint16 elementIndex;
    LightmapElement::GetAtlasAndElementIndex(elementId, atlasIndex, elementIndex);

    if (atlasIndex >= m_atlases.Size())
    {
        return nullptr;
    }

    if (elementIndex >= m_atlases[atlasIndex].elements.Size())
    {
        return nullptr;
    }

    return &m_atlases[atlasIndex].elements[elementIndex];
}

void LightmapVolume::RemoveAllElements()
{
    for (LightmapVolumeAtlas& atlas : m_atlases)
    {
        atlas.Clear();
    }

    Handle<AssetRegistry> assetRegistry = GetCurrentAssetRegistry();
    Assert(assetRegistry.IsValid());

    // Remove textures from their respective packages
    for (Handle<Texture>& texture : m_radianceAtlasTextures)
    {
        if (!texture.IsValid())
            continue;

        assetRegistry->RemoveAsset(texture);

        EnqueueDeletion(std::move(texture));
    }

    for (Handle<Texture>& texture : m_irradianceAtlasTextures)
    {
        if (!texture.IsValid())
            continue;

        assetRegistry->RemoveAsset(texture);

        EnqueueDeletion(std::move(texture));
    }

    m_atlases.Clear();
    m_atlases.EmplaceBack(DefaultAtlasDimensions);

    m_radianceAtlasTextures = {};
    m_irradianceAtlasTextures = {};

    MarkDirty();
    SetNeedsRenderProxyUpdate();
}

const Handle<Texture>& LightmapVolume::GetAtlasTexture(uint16 atlasIndex, AtlasTextureType type) const
{
    if (atlasIndex >= m_atlases.Size())
    {
        AssertDebug(false, "atlas index out of bounds");

        return Handle<Texture>::Null();
    }

    switch (type)
    {
    case RadianceTexture:
        return m_radianceAtlasTextures[atlasIndex];
    case IrradianceTexture:
        return m_irradianceAtlasTextures[atlasIndex];
    default:
        return Handle<Texture>::Null();
    }
}

void LightmapVolume::SetAtlasTexture(uint16 atlasIndex, AtlasTextureType type, const Handle<Texture>& texture)
{
    if (atlasIndex >= m_atlases.Size())
    {
        AssertDebug(false, "atlas index out of bounds");

        return;
    }

    switch (type)
    {
    case RadianceTexture:
        if (texture == m_radianceAtlasTextures[atlasIndex])
        {
            return;
        }

        EnqueueDeletion(std::move(m_radianceAtlasTextures[atlasIndex]));
        m_radianceAtlasTextures[atlasIndex] = texture;

        Check(texture->Create());

        break;
    case IrradianceTexture:
        if (texture == m_irradianceAtlasTextures[atlasIndex])
        {
            return;
        }

        EnqueueDeletion(std::move(m_irradianceAtlasTextures[atlasIndex]));
        m_irradianceAtlasTextures[atlasIndex] = texture;

        Check(texture->Create());

        break;
    default:
        break;
    }
}

void LightmapVolume::OnAddedToWorld(World* world)
{
    VolumeBase::OnAddedToWorld(world);

    const BoundingBox worldBounds = GetWorldBounds();

    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, lightmapElementComponent] : scene->GetEntityManager()->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            if (lightmapElementComponent.lightmapVolume.GetUnsafe() != this
                && lightmapElementComponent.lightmapVolumeName == GetName())
            {
                // Verify the element ID exists in this volume before assigning
                const LightmapElement* lightmapElement = GetElement(lightmapElementComponent.lightmapElementId);

                if (!lightmapElement)
                {
                    HYP_LOG(Lightmap, Warning, "Lightmap element with ID {} does not exist in lightmap volume", lightmapElementComponent.lightmapElementId);
                    continue;
                }

                lightmapElementComponent.lightmapVolume = MakeWeakRef(this);

                entity->SetNeedsRenderProxyUpdate();
            }
        }
    }
}

void LightmapVolume::OnRemovedFromWorld(World* world)
{
    VolumeBase::OnRemovedFromWorld(world);

    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, lightmapElementComponent] : scene->GetEntityManager()->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            if (lightmapElementComponent.lightmapVolume.GetUnsafe() == this)
            {
                lightmapElementComponent.lightmapVolume.Reset();

                entity->SetNeedsRenderProxyUpdate();
            }
        }
    }
}

void LightmapVolume::UpdateRenderProxy(RenderProxyLightmapVolume* proxy)
{
    proxy->lightmapVolume = this;

    proxy->atlasIrradianceTextures = {};

    for (uint32 i = 0; i < uint32(m_irradianceAtlasTextures.Size()); i++)
    {
        proxy->atlasIrradianceTextures[i] = m_irradianceAtlasTextures[i].Get();
    }

    proxy->atlasRadianceTextures = {};

    for (uint32 i = 0; i < uint32(m_radianceAtlasTextures.Size()); i++)
    {
        proxy->atlasRadianceTextures[i] = m_radianceAtlasTextures[i].Get();
    }

    proxy->numAtlases = uint32(m_atlases.Size());

    proxy->bufferData.aabbMax = Vec4f(m_localBounds.max, 1.0f);
    proxy->bufferData.aabbMin = Vec4f(m_localBounds.min, 1.0f);
    proxy->bufferData.textureIndex = ~0u; /// \todo : Set the correct texture index based on the element
}

#if HYP_EDITOR

void LightmapVolume::Rebake()
{
    World* world = GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: not attached to a World", Id());

        return;
    }

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    bakerSubsystem->EnqueueBake(MakeStrongRef(this));
}

#endif

} // namespace Hyperion
