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
#include <Scene/Systems/SwatchOverrideSystem.hpp>

#include <Scene/Swatch.hpp>

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

namespace {

SwatchOverrideSystem* GetSwatchOverrideSystem(const LightmapVolume* volume)
{
    World* world = volume->GetWorld();

    return world ? world->GetSystem<SwatchOverrideSystem>() : nullptr;
}

Name BuildAtlasTextureName(Name volumeName, Name swatchName, uint16 atlasIndex, LightmapVolume::AtlasTextureType type)
{
    return NAME_FMT("LightmapVolumeAtlasTexture_{}_{}_{}_{}",
        volumeName, swatchName, atlasIndex, LightmapVolume::TextureTypeNames[type]);
}

void SyncSwatchOverrideBaseValue(LightmapVolume* volume, LightmapVolume::AtlasTextureType type, const FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume>& textures)
{
    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(volume);

    if (!swatchOverrideSystem)
    {
        return;
    }

    swatchOverrideSystem->SetSwatchOverrideBaseValue(volume, LightmapVolume::GetAtlasTexturesPropertyName(type), BoxedValue(textures));
}

} // namespace

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

            outElement->id = LightmapElement::MakeId(uint16(atlasIndex), uint16(elementIndex));

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

bool LightmapVolume::GetEntityLightmapRect(LightmapElementId elementId, uint32& outRectOffset, uint32& outRectSize, uint8& outStencilValue) const
{
    outRectOffset = 0;
    outRectSize = 0;
    outStencilValue = 0;

    const LightmapElement* element = GetElement(elementId);

    if (!element || !element->IsValid())
    {
        return false;
    }

    const uint16 atlasIndex = element->GetAtlasIndex();
    const Vec2u atlasDimensions = m_atlases[atlasIndex].atlasDimensions;

    if (!MathUtil::IsPowerOfTwo(atlasDimensions.x) || !MathUtil::IsPowerOfTwo(atlasDimensions.y)
        || atlasDimensions.x > 4096 || atlasDimensions.y > 4096)
    {
        HYP_LOG_ONCE(Lightmap, Error, "LightmapVolume '{}' has an atlas of {}x{}; entity rects need power of two atlases no larger than 4096",
            m_name, atlasDimensions.x, atlasDimensions.y);

        return false;
    }

    const uint8 stencilValue = GetStencilValue(atlasIndex);

    if (stencilValue == 0)
    {
        return false;
    }

    // atlasUV = uv1 * scale + offsetUV. The offset sits inside the rect's gutter, and since UV1 is square-normalized
    // the scale can reach past the rect on its shorter axis (UV1 never does)
    const Vec2u offsetTexels {
        MathUtil::Min(uint32(MathUtil::Round(element->offsetUV.x * float(atlasDimensions.x))), 4095u),
        MathUtil::Min(uint32(MathUtil::Round(element->offsetUV.y * float(atlasDimensions.y))), 4095u)
    };

    const Vec2u scaleTexels {
        MathUtil::Clamp(uint32(MathUtil::Round(element->scale.x * float(atlasDimensions.x))), 1u, 4096u),
        MathUtil::Clamp(uint32(MathUtil::Round(element->scale.y * float(atlasDimensions.y))), 1u, 4096u)
    };

    outRectOffset = offsetTexels.x
        | (offsetTexels.y << 12u)
        | (uint32(stencilValue) << 24u);

    outRectSize = ((scaleTexels.x - 1u) & 0xFFFu)
        | (((scaleTexels.y - 1u) & 0xFFFu) << 12u)
        | (uint32(MathUtil::FastLog2(atlasDimensions.x)) << 24u)
        | (uint32(MathUtil::FastLog2(atlasDimensions.y)) << 28u);

    outStencilValue = stencilValue;

    return true;
}

uint32 LightmapVolume::NumUsedAtlases() const
{
    uint32 numUsedAtlases = 0;

    for (uint32 atlasIndex = 0; atlasIndex < uint32(m_atlases.Size()); atlasIndex++)
    {
        if (m_atlases[atlasIndex].elements.Any())
        {
            numUsedAtlases = atlasIndex + 1;
        }
    }

    return numUsedAtlases;
}

void LightmapVolume::SetTexelsPerUnit(float texelsPerUnit)
{
    texelsPerUnit = MathUtil::Max(texelsPerUnit, 0.01f);

    if (m_texelsPerUnit == texelsPerUnit)
    {
        return;
    }

    m_texelsPerUnit = texelsPerUnit;

    MarkDirty();
}

BoundingBox LightmapVolume::GetLightingBounds() const
{
    BoundingBox lightingBounds = GetWorldBounds();

    if (m_coverageBounds.IsValid())
    {
        lightingBounds = lightingBounds.IsValid() ? lightingBounds.Union(m_coverageBounds) : m_coverageBounds;
    }

    return lightingBounds;
}

void LightmapVolume::SetCoverageBounds(const BoundingBox& coverageBounds)
{
    if (m_coverageBounds == coverageBounds)
    {
        return;
    }

    m_coverageBounds = coverageBounds;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void LightmapVolume::SetStencilBase(uint8 stencilBase)
{
    if (m_stencilBase == stencilBase)
    {
        return;
    }

    m_stencilBase = stencilBase;

    SetNeedsRenderProxyUpdate();
}

void LightmapVolume::SetPacking(Array<LightmapVolumeAtlas>&& atlases, float packedTexelsPerUnit)
{
    Assert(atlases.Size() <= MaxAtlasesPerLightmapVolume);

    m_atlases = std::move(atlases);

    if (m_atlases.Empty())
    {
        m_atlases.EmplaceBack(DefaultAtlasDimensions);
    }

    m_packedTexelsPerUnit = packedTexelsPerUnit;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void LightmapVolume::RemoveAllElements(uint32 preserveTextureTypesMask)
{
    ClearSwatchAtlasTextureOverrides(preserveTextureTypesMask);

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
        SyncSwatchOverrideBaseValue(this, IrradianceTexture, m_irradianceAtlasTextures);
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
        SyncSwatchOverrideBaseValue(this, BentNormalTexture, m_bentNormalAtlasTextures);
    }

    m_atlases.Clear();
    m_atlases.EmplaceBack(DefaultAtlasDimensions);

    // The packing is gone - the next bake has to generate a new one.
    m_packedTexelsPerUnit = 0.0f;
    m_coverageBounds = BoundingBox::Empty();

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
    textures[atlasIndex] = texture;

    if (texture.IsValid())
    {
        texture->SetName(BuildAtlasTextureName(m_name, g_defaultSwatchName, atlasIndex, type));
        GetCurrentAssetRegistry()->PutAssetUnique(texture);
    }

    SyncSwatchOverrideBaseValue(this, type, textures);
}

Name LightmapVolume::GetAtlasTexturesPropertyName(AtlasTextureType type)
{
    switch (type)
    {
    case IrradianceTexture:
        return NAME("IrradianceAtlasTextures");
    case BentNormalTexture:
        return NAME("BentNormalAtlasTextures");
    default:
        return Name::Invalid();
    }
}

FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> LightmapVolume::GetAtlasTexturesForSwatch(AtlasTextureType type, Name swatchName) const
{
    FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> baseTextures;

    Span<const Handle<Texture>> currentTextures = GetAtlasTextures(type);

    for (uint32 i = 0; i < MaxAtlasesPerLightmapVolume && i < uint32(currentTextures.Size()); i++)
    {
        baseTextures[i] = currentTextures[i];
    }

    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        return baseTextures;
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        return baseTextures;
    }

    BoxedValue overrideValue;

    if (!swatchOverrideSystem->GetSwatchOverrideValue(this, swatchName, GetAtlasTexturesPropertyName(type), overrideValue))
    {
        return baseTextures;
    }

    using AtlasTextureArray = FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume>;

    if (overrideValue.Is<AtlasTextureArray>())
    {
        return overrideValue.Get<AtlasTextureArray>();
    }

    HYP_LOG(Lightmap, Warning, "Swatch override '{}' on LightmapVolume '{}' is not an atlas texture array",
        swatchName, m_name);

    return baseTextures;
}

void LightmapVolume::SetAtlasTextureForSwatch(uint16 atlasIndex, AtlasTextureType type, const Handle<Texture>& texture, Name swatchName)
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        SetAtlasTexture(atlasIndex, type, texture);

        return;
    }

    if (atlasIndex >= m_atlases.Size())
    {
        AssertDebug(false, "atlas index out of bounds");

        return;
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        HYP_LOG(Lightmap, Error, "Cannot assign atlas texture for swatch '{}' on LightmapVolume '{}': no SwatchOverrideSystem",
            swatchName, m_name);

        return;
    }

    FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> textures = GetAtlasTexturesForSwatch(type, swatchName);

    if (textures[atlasIndex] == texture)
    {
        return;
    }

    textures[atlasIndex] = texture;

    if (texture.IsValid())
    {
        texture->SetName(BuildAtlasTextureName(m_name, swatchName, atlasIndex, type));
        GetCurrentAssetRegistry()->PutAssetUnique(texture);
    }

    swatchOverrideSystem->AddSwatchOverrideSet(this, swatchName);
    swatchOverrideSystem->SetSwatchOverrideValue(this, swatchName, GetAtlasTexturesPropertyName(type), BoxedValue(std::move(textures)));

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void LightmapVolume::ClearSwatchAtlasTextureOverrides(uint32 preserveTextureTypesMask)
{
    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        return;
    }

    for (Name swatchName : swatchOverrideSystem->GetSetSwatchNames(this))
    {
        for (uint32 type = 0; type < NumAtlasTextureTypes; type++)
        {
            if (preserveTextureTypesMask & (1u << type))
            {
                continue;
            }

            swatchOverrideSystem->RemoveSwatchOverrideValue(this, swatchName, GetAtlasTexturesPropertyName(AtlasTextureType(type)));
        }
    }
}

#ifdef HYP_EDITOR
Array<Name> LightmapVolume::GetBakedSwatchNames() const
{
    Array<Name> swatchNames;

    if (m_irradianceAtlasTextures[0].IsValid())
    {
        swatchNames.PushBack(g_defaultSwatchName);
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        return swatchNames;
    }

    const Name propertyName = GetAtlasTexturesPropertyName(IrradianceTexture);

    for (Name swatchName : swatchOverrideSystem->GetSetSwatchNames(this))
    {
        if (swatchOverrideSystem->IsPropertyOverriddenInSwatch(this, swatchName, propertyName))
        {
            swatchNames.PushBack(swatchName);
        }
    }

    return swatchNames;
}
#endif // HYP_EDITOR

void LightmapVolume::OnAddedToWorld(World* world)
{
    VolumeBase::OnAddedToWorld(world);
    
    LightmapSystem* lightmapSystem = world->GetSystem<LightmapSystem>();

    if (!lightmapSystem)
    {
        return;
    }

    if (m_id == InvalidId)
    {
        SetLightmapVolumeId(lightmapSystem->AllocateLightmapVolumeId());
    }
    else
    {
        lightmapSystem->MarkLightmapVolumeIdUsed(m_id);
    }

    lightmapSystem->RegisterVolume(this);

    // entities that entered the world before this volume couldn't resolve to it then
    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, lightmapElementComponent] : scene->GetEntityManager()->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            if (lightmapElementComponent.lightmapVolumeId == m_id)
            {
                lightmapSystem->ResolveVolumeForEntity(*entity, lightmapElementComponent);
            }
        }
    }

    lightmapSystem->AssignStencilValues();
}

void LightmapVolume::OnRemovedFromWorld(World* world)
{
    VolumeBase::OnRemovedFromWorld(world);

    if (LightmapSystem* lightmapSystem = world->GetSystem<LightmapSystem>())
    {
        lightmapSystem->UnregisterVolume(this);

        if (m_id != InvalidId)
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

    m_stencilBase = 0;

    // frees this volume's stencil values for the ones it overlapped
    if (LightmapSystem* lightmapSystem = world->GetSystem<LightmapSystem>())
    {
        lightmapSystem->AssignStencilValues();
    }
}

void LightmapVolume::UpdateRenderProxy(RenderProxyLightmapVolume* proxy)
{
    proxy->lightmapVolume = this;

    for (uint32 i = 0; i < uint32(m_irradianceAtlasTextures.Size()); i++)
    {
        if (proxy->atlasIrradianceTextures[i] != m_irradianceAtlasTextures[i].Get())
        {
            proxy->atlasIrradianceTextures[i] = m_irradianceAtlasTextures[i].Get();

            if (m_irradianceAtlasTextures[i].IsValid())
            {
                // needs full rebind to ensure the texture is uploaded on the render thread.
                proxy->forceRebind = true;
            }
        }
    }

    for (uint32 i = 0; i < uint32(m_bentNormalAtlasTextures.Size()); i++)
    {
        if (proxy->atlasBentNormalTextures[i] != m_bentNormalAtlasTextures[i].Get())
        {
            proxy->atlasBentNormalTextures[i] = m_bentNormalAtlasTextures[i].Get();

            if (m_bentNormalAtlasTextures[i].IsValid())
            {
                // needs full rebind to ensure the texture is uploaded on the render thread.
                proxy->forceRebind = true;
            }
        }
    }

    proxy->numAtlases = uint32(m_atlases.Size());
    proxy->stencilBase = m_stencilBase;

    // owned entities can stick out of the volume, so the lighting box covers them too; the stencil test does the per-pixel routing
    const BoundingBox worldAabb = GetLightingBounds();

    proxy->worldAabb = worldAabb;

    proxy->transformMatrix = worldAabb.IsValid()
        ? Mat4f::Translation(worldAabb.GetCenter()) * Mat4f::Scaling(worldAabb.GetExtent() * 0.5f)
        : Mat4f::Identity();

    proxy->bufferData.aabbMax = Vec4f(worldAabb.max, 1.0f);
    proxy->bufferData.aabbMin = Vec4f(worldAabb.min, 1.0f);
    proxy->bufferData.textureIndex = ~0u; /// \todo : Set the correct texture index based on the element
}

#ifdef HYP_EDITOR

template <Baking::PathTraceType ShadingType>
static void EnqueueBake(LightmapVolume& self)
{
    World* world = self.GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: not attached to a World", self.GetName());

        return;
    }

    const Handle<Swatch>& swatch = world->GetActiveSwatch();

    if (!swatch.IsValid())
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: could not resolve a target swatch for it", self.GetName());

        return;
    }

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    bakerSubsystem->EnqueueBake(swatch->bakeLayer, MakeStrongRef(&self), (1u << uint32(ShadingType)));
}

void LightmapVolume::BakeLightmap()
{
    EnqueueBake<Baking::PathTraceType::Lightmap>(*this);
}

void LightmapVolume::BakeBentNormals()
{
    EnqueueBake<Baking::PathTraceType::BentNormals>(*this);
}

#endif // HYP_EDITOR

} // namespace Hyperion
