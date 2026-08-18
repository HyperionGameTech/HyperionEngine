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

#include <Scene/Systems/LightmapSystem.hpp>

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

#ifdef HYP_EDITOR
#include <Baking/Baker.hpp>
#include <Baking/BakerSubsystem.hpp>
#include <Baking/LightmapVolume/LightmapVolumeBakeData.hpp>
#endif // HYP_EDITOR

#include <LightmapVolume.generated.inl>

namespace Hyperion {

#ifdef HYP_EDITOR
EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
#endif // HYP_EDITOR

LightmapVolume::LightmapVolume()
    : LightmapVolume(BoundingBox::Empty())
{
}

LightmapVolume::LightmapVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds),
      m_irradianceAtlasTextures {},
      m_bentNormalAtlasTextures {},
      m_id(InvalidId)
{
    m_atlases.Reserve(MaxAtlasesPerLightmapVolume);
    m_atlases.EmplaceBack(DefaultAtlasDimensions);
}

LightmapVolume::~LightmapVolume()
{
    if (AnyOf(m_irradianceAtlasTextures, &Handle<Texture>::IsValid))
    {
        EnqueueDeletion(std::move(m_irradianceAtlasTextures));
    }

    if (AnyOf(m_bentNormalAtlasTextures, &Handle<Texture>::IsValid))
    {
        EnqueueDeletion(std::move(m_bentNormalAtlasTextures));
    }
}

void LightmapVolume::SetName(Name name)
{
    if (name == m_name)
    {
        return;
    }

    Node::SetName(name);

    Handle<AssetRegistry> assetRegistry = GetCurrentAssetRegistry();

    // Update altas textures' interpolated names.
    for (uint16 i = 0; i < MaxAtlasesPerLightmapVolume; i++)
    {
        if (m_irradianceAtlasTextures[i].IsValid())
        {
            m_irradianceAtlasTextures[i]->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_{}", name, TextureTypeNames[IrradianceTexture]));

            assetRegistry->PutAssetUnique(m_irradianceAtlasTextures[i]);
        }

        if (m_bentNormalAtlasTextures[i].IsValid())
        {
            m_bentNormalAtlasTextures[i]->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_{}", name, TextureTypeNames[BentNormalTexture]));

            assetRegistry->PutAssetUnique(m_bentNormalAtlasTextures[i]);
        }
    }
}

void LightmapVolume::SetLightmapVolumeId(LightmapVolumeId id)
{
    if (m_id == id)
    {
        return;
    }

    m_id = id;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
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

void LightmapVolume::RemoveAllElements(uint32 preserveTextureTypesMask)
{
    for (LightmapVolumeAtlas& atlas : m_atlases)
    {
        atlas.Clear();
    }

    Handle<AssetRegistry> assetRegistry = GetCurrentAssetRegistry();
    Assert(assetRegistry.IsValid());

    if (!(preserveTextureTypesMask & (1u << IrradianceTexture)))
    {
        for (Handle<Texture>& texture : m_irradianceAtlasTextures)
        {
            if (!texture.IsValid())
            {
                continue;
            }

            assetRegistry->RemoveAsset(texture);

            EnqueueDeletion(std::move(texture));
        }

        m_irradianceAtlasTextures = {};
    }

    if (!(preserveTextureTypesMask & (1u << BentNormalTexture)))
    {
        for (Handle<Texture>& texture : m_bentNormalAtlasTextures)
        {
            if (!texture.IsValid())
            {
                continue;
            }

            assetRegistry->RemoveAsset(texture);

            EnqueueDeletion(std::move(texture));
        }

        m_bentNormalAtlasTextures = {};
    }

    m_atlases.Clear();
    m_atlases.EmplaceBack(DefaultAtlasDimensions);

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

    auto textures = GetAtlasTextures(type);

    return textures[atlasIndex];
}

void LightmapVolume::SetAtlasTexture(uint16 atlasIndex, AtlasTextureType type, const Handle<Texture>& texture)
{
    if (atlasIndex >= m_atlases.Size())
    {
        AssertDebug(false, "atlas index out of bounds");

        return;
    }

    auto& textures = GetAtlasTexturesArray(type);

    if (texture == textures[atlasIndex])
    {
        return;
    }

    SetNeedsRenderProxyUpdate();

    EnqueueDeletion(std::move(textures[atlasIndex]));

    if (!texture.IsValid())
    {
        return;
    }

    textures[atlasIndex] = texture;

    Check(texture->Create());

    texture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_{}", m_name, TextureTypeNames[type]));
    GetCurrentAssetRegistry()->PutAssetUnique(texture);
}

void LightmapVolume::OnAddedToWorld(World* world)
{
    VolumeBase::OnAddedToWorld(world);
    
    if (LightmapSystem* lightmapSystem = world->GetSystem<LightmapSystem>())
    {
        if (m_id == InvalidId)
        {
            SetLightmapVolumeId(lightmapSystem->AllocateLightmapVolumeId());
        }
        else
        {
        
            lightmapSystem->MarkLightmapVolumeIdUsed(m_id);
        }
    }

    const BoundingBox worldBounds = GetWorldBounds();

    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, lightmapElementComponent] : scene->GetEntityManager()->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            if (lightmapElementComponent.lightmapVolume.GetUnsafe() != this
                && lightmapElementComponent.GetTopAssignment() == m_id)
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

    if (m_id != InvalidId)
    {
        if (LightmapSystem* lightmapSystem = world->GetSystem<LightmapSystem>())
        {
            // Doesn't actually free it to be re-used; as we might want to undo removal from the world.
            lightmapSystem->MarkLightmapVolumeIdFreed(m_id);
        }
    }

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

    proxy->atlasBentNormalTextures = {};

    for (uint32 i = 0; i < uint32(m_bentNormalAtlasTextures.Size()); i++)
    {
        proxy->atlasBentNormalTextures[i] = m_bentNormalAtlasTextures[i].Get();
    }

    proxy->numAtlases = uint32(m_atlases.Size());

    const BoundingBox worldAabb = m_localBounds.IsValid()
        ? (GetWorldMatrix() * m_localBounds)
        : BoundingBox::Empty();

    proxy->worldAabb = worldAabb;

    proxy->transformMatrix = GetWorldMatrix()
        * Mat4f::Translation(m_localBounds.GetCenter())
        * Mat4f::Scaling(m_localBounds.GetExtent() * 0.5f);

    proxy->bufferData.aabbMax = Vec4f(worldAabb.max, 1.0f);
    proxy->bufferData.aabbMin = Vec4f(worldAabb.min, 1.0f);
    proxy->bufferData.textureIndex = ~0u; /// \todo : Set the correct texture index based on the element
}

#ifdef HYP_EDITOR

template <Baking::LightmapShadingType ShadingType>
static void EnqueueBake(LightmapVolume& self)
{
    World* world = self.GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: not attached to a World", self.GetName());

        return;
    }

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    bakerSubsystem->EnqueueBake(MakeStrongRef(&self), (1u << uint32(ShadingType)));
}

void LightmapVolume::BakeLightmap()
{
    EnqueueBake<Baking::LightmapShadingType::LIGHTMAP>(*this);
}

void LightmapVolume::BakeBentNormals()
{
    EnqueueBake<Baking::LightmapShadingType::BENT_NORMAL>(*this);
}

#endif // HYP_EDITOR

} // namespace Hyperion
