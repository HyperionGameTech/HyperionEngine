/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/subsystems/sky/SkydomeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <scene/World.hpp>
#include <scene/Texture.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/CameraComponent.hpp>

#include <core/threading/Scheduler.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <asset/Assets.hpp>

#include <Engine.hpp>

namespace hyperion {

SkydomeRenderer::SkydomeRenderer(Vec2u dimensions)
    : m_dimensions(dimensions)
{
}

SkydomeRenderer::~SkydomeRenderer()
{
    HYP_SYNC_RENDER(); // wait for render commands to finish
}

void SkydomeRenderer::Init()
{
    m_cubemap = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_CUBEMAP,
        InternalFormat::R11G11B10F,
        Vec3u { m_dimensions.x, m_dimensions.y, 1 },
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        FilterMode::TEXTURE_FILTER_LINEAR });

    InitObject(m_cubemap);
    m_cubemap->SetPersistentRenderResourceEnabled(true);

    m_virtual_scene = CreateObject<Scene>(SceneFlags::NONE);
    m_virtual_scene->SetOwnerThreadID(g_game_thread);
    m_virtual_scene->SetName(Name::Unique("SkydomeRendererScene"));
    InitObject(m_virtual_scene);

    m_camera = CreateObject<Camera>(
        90.0f,
        -int(m_dimensions.x), int(m_dimensions.y),
        0.1f, 10000.0f);

    m_camera->SetName(Name::Unique("SkydomeRendererCamera"));
    m_camera->SetViewMatrix(Matrix4::LookAt(Vec3f::UnitZ(), Vec3f::Zero(), Vec3f::UnitY()));

    InitObject(m_camera);

    Handle<Node> camera_node = m_virtual_scene->GetRoot()->AddChild();
    camera_node->SetName(m_camera->GetName());

    Handle<Entity> camera_entity = m_virtual_scene->GetEntityManager()->AddEntity();
    m_virtual_scene->GetEntityManager()->AddTag<EntityTag::CAMERA_PRIMARY>(camera_entity);
    m_virtual_scene->GetEntityManager()->AddComponent<CameraComponent>(camera_entity, CameraComponent { m_camera });

    camera_node->SetEntity(camera_entity);

    m_env_probe = m_virtual_scene->GetEntityManager()->AddEntity<EnvProbe>(BoundingBox(Vec3f(-100.0f), Vec3f(100.0f)), m_dimensions, EnvProbeType::SKY);
    InitObject(m_env_probe);

    auto dome_node_asset = g_asset_manager->Load<Node>("models/inv_sphere.obj");

    if (dome_node_asset.HasValue())
    {
        Handle<Node> dome_node = dome_node_asset->Result();
        dome_node->Scale(Vec3f(10.0f));
        dome_node->LockTransform();

        m_virtual_scene->GetRoot()->AddChild(dome_node);
    }
}

void SkydomeRenderer::OnAddedToWorld()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    AssertDebug(m_virtual_scene.IsValid());
    AssertDebug(m_camera.IsValid());
    AssertDebug(m_cubemap.IsValid());

    GetWorld()->AddScene(m_virtual_scene);
}

void SkydomeRenderer::OnRemovedFromWorld()
{
    if (m_env_probe)
    {
        m_env_probe.Reset();
    }

    m_camera.Reset();
    m_cubemap.Reset();

    GetWorld()->RemoveScene(m_virtual_scene);
}

void SkydomeRenderer::Update(float delta)
{
    struct RENDER_COMMAND(RenderSky)
        : public renderer::RenderCommand
    {
        RenderWorld* world;
        RenderTexture* cubemap;

        Handle<EnvProbe> env_probe;

        RENDER_COMMAND(RenderSky)(RenderWorld* world, RenderTexture* cubemap, const Handle<EnvProbe>& env_probe)
            : world(world),
              env_probe(env_probe),
              cubemap(cubemap)
        {
        }

        virtual ~RENDER_COMMAND(RenderSky)() override
        {
            world->DecRef();
            cubemap->DecRef();
            env_probe->GetRenderResource().DecRef();
        }

        virtual RendererResult operator()() override
        {
            FrameBase* frame = g_rendering_api->GetCurrentFrame();

            RenderSetup render_setup { world, nullptr };

            HYP_LOG(Rendering, Debug, "Rendering sky with env probe: {}", env_probe->GetID());

            env_probe->GetRenderResource().Render(frame, render_setup);

            // Copy cubemap from env probe to cubemap texture
            const ImageRef& src_image = env_probe->GetView()->GetOutputTarget()->GetAttachment(0)->GetImage();

            AssertThrow(src_image.IsValid());
            AssertThrow(src_image->IsCreated());

            const ImageRef& dst_image = cubemap->GetImage();

            frame->GetCommandList().Add<InsertBarrier>(src_image, renderer::ResourceState::COPY_SRC);
            frame->GetCommandList().Add<InsertBarrier>(dst_image, renderer::ResourceState::COPY_DST);

            frame->GetCommandList().Add<Blit>(src_image, dst_image);

            frame->GetCommandList().Add<GenerateMipmaps>(dst_image);

            frame->GetCommandList().Add<InsertBarrier>(src_image, renderer::ResourceState::SHADER_RESOURCE);
            frame->GetCommandList().Add<InsertBarrier>(dst_image, renderer::ResourceState::SHADER_RESOURCE);

            HYPERION_RETURN_OK;
        }
    };

    if (!m_env_probe->ReceivesUpdate())
    {
        return;
    }

    m_env_probe->Update(delta);
    m_env_probe->SetNeedsRender(true);

    if (m_env_probe->NeedsRender())
    {
        g_engine->GetWorld()->GetRenderResource().IncRef();
        m_cubemap->GetRenderResource().IncRef();
        m_env_probe->GetRenderResource().IncRef();

        PUSH_RENDER_COMMAND(
            RenderSky,
            &g_engine->GetWorld()->GetRenderResource(),
            &m_cubemap->GetRenderResource(),
            m_env_probe);
    }

    m_env_probe->SetReceivesUpdate(false);
}

} // namespace hyperion