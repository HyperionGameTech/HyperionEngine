/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <system/AppContext.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/World.hpp>

#include <scene/camera/PerspectiveCamera.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

ReflectionProbeRenderer::ReflectionProbeRenderer(Name name, const Handle<EnvProbe>& env_probe)
    : RenderSubsystem(name),
      m_env_probe(env_probe)
{
}

ReflectionProbeRenderer::~ReflectionProbeRenderer()
{
}

void ReflectionProbeRenderer::Init()
{
    InitObject(m_env_probe);
}

void ReflectionProbeRenderer::OnRemoved()
{
    HYP_SYNC_RENDER(); // to prevent dangling pointers
}

void ReflectionProbeRenderer::OnUpdate(float delta)
{
    struct RENDER_COMMAND(RenderReflectionProbe)
        : renderer::RenderCommand
    {
        RenderWorld* world;
        RenderEnvProbe* env_probe;

        RENDER_COMMAND(RenderReflectionProbe)(RenderWorld* world, RenderEnvProbe* env_probe)
            : world(world),
              env_probe(env_probe)
        {
            // world->IncRef();
            // env_probe->IncRef();
        }

        virtual ~RENDER_COMMAND(RenderReflectionProbe)() override
        {
            world->DecRef();
            env_probe->DecRef();
        }

        virtual RendererResult operator()() override
        {
            FrameBase* frame = g_rendering_api->GetCurrentFrame();

            RenderSetup render_setup { world, nullptr };

            env_probe->Render(frame, render_setup);

            HYPERION_RETURN_OK;
        }
    };

    if (!m_env_probe.IsValid())
    {
        return;
    }

    if (!m_env_probe->NeedsRender())
    {
        return;
    }

    g_engine->GetWorld()->GetRenderResource().IncRef();
    m_env_probe->GetRenderResource().IncRef();
    PUSH_RENDER_COMMAND(RenderReflectionProbe, &g_engine->GetWorld()->GetRenderResource(), &m_env_probe->GetRenderResource());

    m_env_probe->SetNeedsRender(false);
}

} // namespace hyperion
