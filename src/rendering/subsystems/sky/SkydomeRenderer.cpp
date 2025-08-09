/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/subsystems/sky/SkydomeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/RenderFrame.hpp>

#include <scene/World.hpp>
#include <rendering/Texture.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>

#include <scene/EntityManager.hpp>

#include <core/threading/Scheduler.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <asset/Assets.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

SkydomeRenderer::SkydomeRenderer(Vec2u dimensions)
    : m_dimensions(dimensions)
{
}

SkydomeRenderer::~SkydomeRenderer()
{
}

void SkydomeRenderer::Init()
{
    m_cubemap = CreateObject<Texture>(TextureDesc {
        TT_CUBEMAP,
        TF_R11G11B10F,
        Vec3u { m_dimensions.x, m_dimensions.y, 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR });

    InitObject(m_cubemap);

    m_virtualScene = CreateObject<Scene>(SceneFlags::NONE);
    m_virtualScene->SetOwnerThreadId(g_gameThread);
    m_virtualScene->SetName(Name::Unique("SkydomeRendererScene"));
    InitObject(m_virtualScene);

    m_camera = CreateObject<Camera>(
        90.0f,
        -int(m_dimensions.x), int(m_dimensions.y),
        0.1f, 10000.0f);

    m_camera->SetName(Name::Unique("SkydomeRendererCamera"));
    m_camera->SetViewMatrix(Matrix4::LookAt(Vec3f::UnitZ(), Vec3f::Zero(), Vec3f::UnitY()));

    m_virtualScene->GetRoot()->AddChild(m_camera);
    m_virtualScene->GetEntityManager()->AddTag<EntityTag::CAMERA_PRIMARY>(m_camera);
    InitObject(m_camera);

    m_envProbe = CreateObject<SkyProbe>(BoundingBox(Vec3f(-100.0f), Vec3f(100.0f)), m_dimensions);
    m_virtualScene->GetRoot()->AddChild(m_envProbe);
    InitObject(m_envProbe);

    auto domeNodeAsset = g_assetManager->Load<Node>("models/inv_sphere.obj");

    if (domeNodeAsset.HasValue())
    {
        Handle<Node> domeNode = domeNodeAsset->Result();
        domeNode->Scale(Vec3f(10.0f));
        domeNode->LockTransform();

        m_virtualScene->GetRoot()->AddChild(domeNode);
    }
}

void SkydomeRenderer::OnAddedToWorld()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    AssertDebug(m_virtualScene.IsValid());
    AssertDebug(m_camera.IsValid());
    AssertDebug(m_cubemap.IsValid());

    GetWorld()->AddScene(m_virtualScene);

    m_envProbe->SetNeedsRender(true);
}

void SkydomeRenderer::OnRemovedFromWorld()
{
    if (m_envProbe)
    {
        m_envProbe.Reset();
    }

    m_camera.Reset();
    m_cubemap.Reset();

    GetWorld()->RemoveScene(m_virtualScene);
}

void SkydomeRenderer::Update(float delta)
{
    if (!m_envProbe->ReceivesUpdate())
    {
        return;
    }

    //    m_envProbe->Update(delta);
    m_envProbe->SetNeedsRender(true);

    // m_envProbe->SetReceivesUpdate(false);
}

} // namespace hyperion
