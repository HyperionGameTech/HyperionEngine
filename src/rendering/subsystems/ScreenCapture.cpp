/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/subsystems/ScreenCapture.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderGlobalState.hpp>
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
          TT_TEX2D,
          TF_RGBA16F,
          Vec3u { Vec2u(view->GetViewport().extent), 1 },
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE })),
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

    m_buffer = g_render_backend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, m_texture->GetRenderResource().GetImage()->GetByteSize());
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
        : RenderCommand
    {
        WeakHandle<ScreenCaptureRenderSubsystem> subsystem_weak;

        RENDER_COMMAND(UpdateScreenCapture)(const WeakHandle<ScreenCaptureRenderSubsystem>& subsystem_weak)
            : subsystem_weak(subsystem_weak)
        {
        }

        virtual ~RENDER_COMMAND(UpdateScreenCapture)() override
        {
        }

        virtual RendererResult operator()() override
        {
            Handle<ScreenCaptureRenderSubsystem> subsystem = subsystem_weak.Lock();
            if (!subsystem.IsValid())
            {
                HYP_LOG(Rendering, Warning, "ScreenCaptureRenderSubsystem is no longer valid. Skipping capture.");
                HYPERION_RETURN_OK;
            }

            FrameBase* frame = g_render_backend->GetCurrentFrame();
            subsystem->CaptureFrame(frame);

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(UpdateScreenCapture, WeakHandle<ScreenCaptureRenderSubsystem>(WeakHandleFromThis()));
}

void ScreenCaptureRenderSubsystem::CaptureFrame(FrameBase* frame)
{
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_texture.IsValid());
    AssertThrow(m_texture->IsReady());

    DeferredRenderer* deferred_renderer = static_cast<DeferredRenderer*>(g_render_global_state->Renderer);
    AssertDebug(deferred_renderer != nullptr);

    DeferredPassData* pd = deferred_renderer->GetLastFrameData().GetPassDataForView(m_view.Get());

    if (!pd)
    {
        HYP_LOG(Rendering, Warning, "No pass data found for view {}. Skipping screen capture.", m_view->Id());

        return;
    }

    const ImageRef& image_ref = deferred_renderer->GetRendererConfig().taa_enabled
        ? pd->temporal_aa->GetResultTexture()->GetRenderResource().GetImage()
        : pd->tonemap_pass->GetFinalImageView()->GetImage();

    AssertThrow(image_ref.IsValid());

    // wait for the image to be ready
    if (image_ref->GetResourceState() == RS_UNDEFINED)
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

    const ResourceState previous_resource_state = image_ref->GetResourceState();

    frame->GetCommandList().Add<InsertBarrier>(image_ref, RS_COPY_SRC);

    switch (m_screen_capture_mode)
    {
    case ScreenCaptureMode::TO_TEXTURE:
        AssertThrow(m_texture->GetRenderResource().GetImage()->IsCreated());

        frame->GetCommandList().Add<InsertBarrier>(m_texture->GetRenderResource().GetImage(), RS_COPY_DST);
        frame->GetCommandList().Add<Blit>(image_ref, m_texture->GetRenderResource().GetImage());
        frame->GetCommandList().Add<InsertBarrier>(m_texture->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);

        break;
    case ScreenCaptureMode::TO_BUFFER:
        AssertThrow(m_buffer.IsValid() && m_buffer->Size() >= image_ref->GetByteSize());

        frame->GetCommandList().Add<InsertBarrier>(m_buffer, RS_COPY_DST);
        frame->GetCommandList().Add<CopyImageToBuffer>(image_ref, m_buffer);
        frame->GetCommandList().Add<InsertBarrier>(m_buffer, RS_COPY_SRC);

        break;
    default:
        HYP_THROW("Invalid screen capture mode");
    }

    frame->GetCommandList().Add<InsertBarrier>(image_ref, previous_resource_state);
}

} // namespace hyperion