/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>
#include <Scene/WorldGrid/Terrain/TerrainMeshBuilder.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Rendering/Material.hpp>

#include <Framework/EngineGlobals.hpp>

#include <TerrainWorldGridLayer.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

#pragma region TerrainWorldGridLayer

TerrainWorldGridLayer::TerrainWorldGridLayer()
    : m_scene(MakeHandle<Scene>(NAME("TerrainScene"), SceneFlags::FOREGROUND | SceneFlags::HAS_OCTREE))
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
    m_scene->Initialize();

    MaterialAttributes attributes;
    attributes.bucket = RenderBucket::Opaque;
    attributes.flags |= MAF_DEPTH_TEST | MAF_DEPTH_WRITE;

    MaterialParameters parameters;
    parameters.albedo = Vec4f(0.06f, 0.25f, 0.05f, 1.0f);
    parameters.roughness = 0.95f;
    parameters.metalness = 0.0f;

    m_material = g_materialCache->GetOrCreate(NAME("terrain_material"), attributes, parameters, MaterialTextures {});

    GetCurrentAssetRegistry()->PutAsset(m_material);

    TerrainMeshBuilder meshBuilder(m_layerInfo.cellSize);
    m_mesh = meshBuilder.GetMesh();
    Assert(m_mesh.IsValid());

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

    return MakeHandle<TerrainStreamingCell>(cellInfo, m_scene, m_mesh, m_material);
}

#pragma endregion TerrainWorldGridLayer

} // namespace Hyperion
