/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/LightmapVolume.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>

#include <scene/components/LightmapElementComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/threading/Threads.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>

#if HYP_EDITOR
#include <baking/BakerSubsystem.hpp>
#include <baking/lightmap_volume/LightmapVolumeBakeData.hpp>
#endif

#include <LightmapVolume.generated.inl>

namespace Hyperion {

#if HYP_EDITOR
HYP_DECLARE_LOG_CHANNEL(Editor);
#endif

LightmapVolume::LightmapVolume()
    : LightmapVolume(BoundingBox::Empty())
{
}

LightmapVolume::LightmapVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds)
{
    m_atlases.Reserve(MaxAtlases);
    m_atlases.EmplaceBack(DefaultAtlasDimensions);

    m_radianceAtlasTextures.PushBack(Handle<Texture>::Null());
    m_irradianceAtlasTextures.PushBack(Handle<Texture>::Null());
}

LightmapVolume::~LightmapVolume()
{
    EnqueueDeletion(std::move(m_radianceAtlasTextures));
    EnqueueDeletion(std::move(m_irradianceAtlasTextures));
}

bool LightmapVolume::AddElement(Vec2u dimensions, LightmapElement& outElement, bool shrinkToFit, float downscaleLimit)
{
    outElement.id = InvalidLightmapElementId;

    Optional<LightmapVolumeAtlas> tmpAtlas;

    for (uint32 atlasIndex = 0; atlasIndex < MaxAtlases; atlasIndex++)
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

            outElement.id = LightmapElementId(uint32((atlasIndex << 16) | elementIndex));

            // ensure ID is also stored in elements
            atlas->elements[elementIndex].id = outElement.id;

            if (isNewAtlas)
            {
                m_atlases.Resize(MathUtil::Max(m_atlases.Size(), atlasIndex + 1));
                m_radianceAtlasTextures.Resize(m_atlases.Size());
                m_irradianceAtlasTextures.Resize(m_atlases.Size());

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

    m_radianceAtlasTextures.Clear();
    m_irradianceAtlasTextures.Clear();
    m_atlases.Clear();

    MarkDirty();
    SetNeedsRenderProxyUpdate();
}

void LightmapVolume::Init()
{
    VolumeBase::Init();

    for (const Handle<Texture>& texture : m_radianceAtlasTextures)
    {
        if (texture)
        {
            CheckResult(texture->Create());
        }
    }

    for (const Handle<Texture>& texture : m_irradianceAtlasTextures)
    {
        if (texture)
        {
            CheckResult(texture->Create());
        }
    }

    SetReady(true);
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
        EnqueueDeletion(std::move(m_radianceAtlasTextures[atlasIndex]));
        m_radianceAtlasTextures[atlasIndex] = texture;

        if (IsInitCalled())
        {
            CheckResult(texture->Create());
        }

        break;
    case IrradianceTexture:
        EnqueueDeletion(std::move(m_irradianceAtlasTextures[atlasIndex]));
        m_irradianceAtlasTextures[atlasIndex] = texture;

        if (IsInitCalled())
        {
            CheckResult(texture->Create());
        }

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
        // No two LightmapVolumes should overlap in the world
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ))
        {
            LightmapVolume* otherLightmapVolume = static_cast<LightmapVolume*>(entity);

            if (otherLightmapVolume->GetWorldBounds().Overlaps(worldBounds))
            {
                HYP_LOG(Scene, Error, "LightmapVolume {} overlaps with other LightmapVolume {}! This could cause incorrect lightmaps to be applied to entities in the scene!",
                    otherLightmapVolume->GetName(),
                    GetName());
            }
        }

        for (auto [entity, lightmapElementComponent, boundingBoxComponent] : scene->GetEntityManager()->GetEntitySet<LightmapElementComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            if (!lightmapElementComponent.lightmapVolume.IsValid() && boundingBoxComponent.worldAabb.Overlaps(worldBounds))
            {
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
    proxy->lightmapVolume = WeakHandleFromThis();

    proxy->atlasRadianceTextures.Clear();
    proxy->atlasIrradianceTextures.Resize(m_irradianceAtlasTextures.Size());

    for (uint32 i = 0; i < uint32(m_irradianceAtlasTextures.Size()); i++)
    {
        proxy->atlasIrradianceTextures[i] = m_irradianceAtlasTextures[i].Get();
    }

    proxy->atlasRadianceTextures.Clear();
    proxy->atlasRadianceTextures.Resize(m_radianceAtlasTextures.Size());

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
    HYP_SCOPE;

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
