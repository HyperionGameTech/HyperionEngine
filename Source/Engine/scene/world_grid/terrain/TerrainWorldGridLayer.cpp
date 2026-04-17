/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridLayer.hpp>
#include <scene/world_grid/terrain/TerrainStreamingCell.hpp>

#include <scene/world_grid/WorldGrid.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <rendering/MaterialDefinition.hpp>
#include <rendering/MaterialInstance.hpp>

#include <engine/EngineGlobals.hpp>

#include <TerrainWorldGridLayer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(WorldGrid);

#pragma region TerrainWorldGridLayer

TerrainWorldGridLayer::TerrainWorldGridLayer()
    : m_scene(MakeHandle<Scene>(NAME("TerrainScene"), SceneFlags::FOREGROUND))
{
}

TerrainWorldGridLayer::~TerrainWorldGridLayer()
{
}

void TerrainWorldGridLayer::Init()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    WorldGridLayer::Init();

    AssertDebug(m_scene.IsValid());
    InitObject(m_scene);

    MaterialAttributes attributes;
    attributes.bucket = RenderBucket::Opaque;
    attributes.flags |= MAF_DEPTH_TEST | MAF_DEPTH_WRITE;

    MaterialParameters parameters;
    parameters.albedo = Vec4f(0.06f, 0.25f, 0.05f, 1.0f);
    parameters.roughness = 0.95f;
    parameters.metalness = 0.0f;

    m_material = g_materialInstanceCache->GetOrCreate(NAME("terrain_material"), attributes, parameters, MaterialTextures{});
    
    g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/MaterialInstances", m_material);

    // if (auto albedoTextureAsset = AssetManager::GetInstance()->Load<Texture>("textures/mossy-ground1-Unity/mossy-ground1-albedo.png"))
    // {
    //     Handle<Texture> albedoTexture = albedoTextureAsset->Result();

    //     TextureDesc textureDesc = albedoTexture->GetTextureDesc();
    //     textureDesc.format = TextureFormat::RGBA8_SRGB;
    //     albedoTexture->SetTextureDesc(textureDesc);

    //     m_material->SetTexture(MaterialTextureKey::Diffuse, albedoTexture);
    // }

    // if (auto groundTextureAsset = AssetManager::GetInstance()->Load<Texture>("textures/mossy-ground1-Unity/mossy-ground1-preview.png"))
    // {
    //     m_material->SetTexture(MaterialTextureKey::Normals, groundTextureAsset->Result());
    // }

    InitObject(m_material);
}

void TerrainWorldGridLayer::OnAdded_Impl(WorldGrid* worldGrid)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    AssertDebug(worldGrid != nullptr);
    AssertDebug(m_scene.IsValid());

    worldGrid->GetWorld()->AddScene(m_scene);
}

void TerrainWorldGridLayer::OnRemoved_Impl(WorldGrid* worldGrid)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    AssertDebug(worldGrid != nullptr);
    AssertDebug(m_scene.IsValid());

    worldGrid->GetWorld()->RemoveScene(m_scene);
}

Handle<StreamingCell> TerrainWorldGridLayer::CreateStreamingCell_Impl(const StreamingCellInfo& cellInfo)
{
    if (!m_scene)
    {
        return Handle<StreamingCell>::Null();
    }

    return MakeHandle<TerrainStreamingCell>(cellInfo, m_scene, m_material);
}

#pragma endregion TerrainWorldGridLayer

} // namespace Hyperion
