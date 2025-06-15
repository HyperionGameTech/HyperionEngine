/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/subsystems/ScreenCapture.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/TemporalAA.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <scene/Texture.hpp>
#include <scene/World.hpp> // temp

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

ScreenCaptureRenderSubsystem::ScreenCaptureRenderSubsystem(Name name, const TResourceHandle<RenderView>& view, ScreenCaptureMode screen_capture_mode)
    : RenderSubsystem(name),
      m_view(view),
      m_texture(CreateObject<Texture>(TextureDesc {
          ImageType::TEXTURE_TYPE_2D,
          InternalFormat::RGBA16F,
          Vec3u { Vec2u(view->GetViewport().extent), 1 },
          FilterMode::TEXTURE_FILTER_NEAREST,
          FilterMode::TEXTURE_FILTER_NEAREST,
          WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE })),
      m_screen_capture_mode(screen_capture_mode)
{
    InitObject(m_texture);
}

ScreenCaptureRenderSubsystem::~ScreenCaptureRenderSubsystem()
{
    SafeRelease(std::move(m_buffer));
}

void ScreenCaptureRenderSubsystem::Init()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_view);

    m_texture->SetPersistentRenderResourceEnabled(true);

    m_buffer = g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::STAGING_BUFFER, m_texture->GetRenderResource().GetImage()->GetByteSize());
    HYPERION_ASSERT_RESULT(m_buffer->Create());
}

void ScreenCaptureRenderSubsystem::InitGame()
{
}

void ScreenCaptureRenderSubsystem::OnRemoved()
{
    SafeRelease(std::move(m_buffer));

    m_texture->SetPersistentRenderResourceEnabled(false);
}

void ScreenCaptureRenderSubsystem::OnUpdate(GameCounter::TickUnit delta)
{
    // Do nothing
}

void ScreenCaptureRenderSubsystem::OnRender(FrameBase* frame, const RenderSetup&)
{
    AssertThrow(m_texture.IsValid());
    AssertThrow(m_texture->IsReady());

    const ImageRef& image_ref = m_view->GetRendererConfig().taa_enabled
        ? m_view->GetTemporalAA()->GetResultTexture()->GetRenderResource().GetImage()
        : m_view->GetTonemapPass()->GetFinalImageView()->GetImage();

    AssertThrow(image_ref.IsValid());

    // wait for the image to be ready
    if (image_ref->GetResourceState() == renderer::ResourceState::UNDEFINED)
    {
        return;
    }

    if (m_texture->GetExtent() != image_ref->GetExtent())
    {
        m_texture->Resize(image_ref->GetExtent());
    }

    const renderer::ResourceState previous_resource_state = image_ref->GetResourceState();

    frame->GetCommandList().Add<InsertBarrier>(image_ref, renderer::ResourceState::COPY_SRC);

    switch (m_screen_capture_mode)
    {
    case ScreenCaptureMode::TO_TEXTURE:
        // Hack, but we need to make sure the image is created before we can blit to it
        if (!m_texture->GetRenderResource().GetImage()->IsCreated())
        {
            HYPERION_ASSERT_RESULT(renderer::RenderCommands::Flush());
        }

        frame->GetCommandList().Add<InsertBarrier>(m_texture->GetRenderResource().GetImage(), renderer::ResourceState::COPY_DST);
        frame->GetCommandList().Add<Blit>(image_ref, m_texture->GetRenderResource().GetImage());
        frame->GetCommandList().Add<InsertBarrier>(m_texture->GetRenderResource().GetImage(), renderer::ResourceState::SHADER_RESOURCE);

        break;
    case ScreenCaptureMode::TO_BUFFER:
        AssertThrow(m_buffer.IsValid() && m_buffer->Size() >= image_ref->GetByteSize());

        frame->GetCommandList().Add<InsertBarrier>(m_buffer, renderer::ResourceState::COPY_DST);
        frame->GetCommandList().Add<CopyImageToBuffer>(image_ref, m_buffer);
        frame->GetCommandList().Add<InsertBarrier>(m_buffer, renderer::ResourceState::COPY_SRC);

        break;
    default:
        HYP_THROW("Invalid screen capture mode");
    }

    frame->GetCommandList().Add<InsertBarrier>(image_ref, previous_resource_state);
}

} // namespace hyperion