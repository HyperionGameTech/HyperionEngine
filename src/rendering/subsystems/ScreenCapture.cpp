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
#include <scene/World.hpp>
#include <scene/View.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

ScreenCaptureRenderSubsystem::ScreenCaptureRenderSubsystem(const Handle<View>& view, ScreenCaptureMode screen_capture_mode)
    : m_view(view),
      m_texture(CreateObject<Texture>(TextureDesc {
          ImageType::TEXTURE_TYPE_2D,
          InternalFormat::RGBA16F,
          Vec3u { Vec2u(view->GetViewport().extent), 1 },
          FilterMode::TEXTURE_FILTER_NEAREST,
          FilterMode::TEXTURE_FILTER_NEAREST,
          WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE })),
      m_screen_capture_mode(screen_capture_mode)
{
}

ScreenCaptureRenderSubsystem::~ScreenCaptureRenderSubsystem()
{
    HYP_SYNC_RENDER(); // wait for render commands to finish

    SafeRelease(std::move(m_buffer));

    m_view->GetRenderResource().DecRef();
}

void ScreenCaptureRenderSubsystem::Init()
{
    AssertThrow(m_view.IsValid());

    InitObject(m_view);
    m_view->GetRenderResource().IncRef();

    InitObject(m_texture);

    SetReady(true);
}

void ScreenCaptureRenderSubsystem::OnAddedToWorld()
{
    HYP_SCOPE;

    AssertThrow(m_view);

    m_texture->SetPersistentRenderResourceEnabled(true);

    m_buffer = g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::STAGING_BUFFER, m_texture->GetRenderResource().GetImage()->GetByteSize());
    DeferCreate(m_buffer);
}

void ScreenCaptureRenderSubsystem::OnRemovedFromWorld()
{
    SafeRelease(std::move(m_buffer));

    m_texture->SetPersistentRenderResourceEnabled(false);
}

void ScreenCaptureRenderSubsystem::Update(float delta)
{
    struct RENDER_COMMAND(UpdateScreenCapture)
        : renderer::RenderCommand
    {
        ScreenCaptureRenderSubsystem* subsystem;

        RENDER_COMMAND(UpdateScreenCapture)(ScreenCaptureRenderSubsystem* subsystem)
            : subsystem(subsystem)
        {
        }

        virtual ~RENDER_COMMAND(UpdateScreenCapture)() override
        {
        }

        virtual RendererResult operator()() override
        {
            FrameBase* frame = g_rendering_api->GetCurrentFrame();

            subsystem->CaptureFrame(frame);

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(UpdateScreenCapture, this);
}

void ScreenCaptureRenderSubsystem::CaptureFrame(FrameBase* frame)
{
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_texture.IsValid());
    AssertThrow(m_texture->IsReady());

    const ImageRef& image_ref = m_view->GetRenderResource().GetRendererConfig().taa_enabled
        ? m_view->GetRenderResource().GetTemporalAA()->GetResultTexture()->GetRenderResource().GetImage()
        : m_view->GetRenderResource().GetTonemapPass()->GetFinalImageView()->GetImage();

    AssertThrow(image_ref.IsValid());

    // wait for the image to be ready
    if (image_ref->GetResourceState() == renderer::ResourceState::UNDEFINED)
    {
        HYP_LOG(Rendering, Warning, "Screen capture image is not ready. Skipping capture.");
        return;
    }

    if (m_texture->GetExtent() != image_ref->GetExtent())
    {
        // HYP_LOG(Rendering, Debug, "Resizing screen capture texture from {} to {}", m_texture->GetExtent(), image_ref->GetExtent());

        // DelegateHandler* delegate_handle = new DelegateHandler();
        // *delegate_handle = frame->OnFrameEnd.Bind([delegate_handle, weak_this = WeakHandleFromThis(), new_extent = image_ref->GetExtent()](...)
        //     {
        //         if (Handle<ScreenCaptureRenderSubsystem> subsystem = weak_this.Lock().Cast<ScreenCaptureRenderSubsystem>())
        //         {
        //             AssertThrow(subsystem->m_texture.IsValid());

        //             subsystem->m_texture->Resize(new_extent);

        //             subsystem->OnTextureResize(subsystem->m_texture);
        //         }

        //         delete delegate_handle;
        //     });
    }

    const renderer::ResourceState previous_resource_state = image_ref->GetResourceState();

    frame->GetCommandList().Add<InsertBarrier>(image_ref, renderer::ResourceState::COPY_SRC);

    switch (m_screen_capture_mode)
    {
    case ScreenCaptureMode::TO_TEXTURE:
        AssertThrow(m_texture->GetRenderResource().GetImage()->IsCreated());

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