/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridLayer.hpp>
#include <scene/world_grid/terrain/TerrainStreamingCell.hpp>

#include <scene/world_grid/WorldGrid.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <rendering/Material.hpp>

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

    m_material = MakeHandle<Material>(NAME("terrain_material"));
    m_material->SetBucket(RB_OPAQUE);
    m_material->SetIsDepthTestEnabled(true);
    m_material->SetIsDepthWriteEnabled(true);
    m_material->SetParameter(MATERIAL_KEY_ALBEDO, Vec4f(0.06f, 0.25f, 0.05f, 1.0f));
    m_material->SetParameter(MATERIAL_KEY_ROUGHNESS, 0.95f);
    m_material->SetParameter(MATERIAL_KEY_METALNESS, 0.0f);
    m_material->SetParameter(MATERIAL_KEY_UV_SCALE, Vec2f(10.0f));

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
