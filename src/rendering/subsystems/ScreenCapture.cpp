/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/subsystems/ScreenCapture.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/TemporalAA.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/Texture.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

ScreenCaptureRenderSubsystem::ScreenCaptureRenderSubsystem(const Handle<View>& view, ScreenCaptureMode screenCaptureMode)
    : m_view(view),
      m_texture(CreateObject<Texture>(TextureDesc {
          TT_TEX2D,
          TF_RGBA16F,
          Vec3u { Vec2u(view->GetViewport().extent), 1 },
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE })),
      m_screenCaptureMode(screenCaptureMode)
{
}

ScreenCaptureRenderSubsystem::~ScreenCaptureRenderSubsystem()
{
    SafeRelease(std::move(m_buffer));
}

void ScreenCaptureRenderSubsystem::Init()
{
    Assert(m_view.IsValid());

    InitObject(m_view);
    InitObject(m_texture);

    SetReady(true);
}

void ScreenCaptureRenderSubsystem::OnAddedToWorld()
{
    HYP_SCOPE;

    Assert(m_view);

    m_buffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, m_texture->GetGpuImage()->GetByteSize());
    DeferCreate(m_buffer);
}

void ScreenCaptureRenderSubsystem::OnRemovedFromWorld()
{
    SafeRelease(std::move(m_buffer));
}

void ScreenCaptureRenderSubsystem::Update(float delta)
{
    struct RENDER_COMMAND(UpdateScreenCapture) : RenderCommand
    {
        WeakHandle<ScreenCaptureRenderSubsystem> subsystemWeak;

        RENDER_COMMAND(UpdateScreenCapture)(const WeakHandle<ScreenCaptureRenderSubsystem>& subsystemWeak)
            : subsystemWeak(subsystemWeak)
        {
        }

        virtual ~RENDER_COMMAND(UpdateScreenCapture)() override
        {
        }

        virtual RendererResult operator()() override
        {
            Handle<ScreenCaptureRenderSubsystem> subsystem = subsystemWeak.Lock();
            if (!subsystem.IsValid())
            {
                HYP_LOG(Rendering, Warning, "ScreenCaptureRenderSubsystem is no longer valid. Skipping capture.");
                HYPERION_RETURN_OK;
            }

            FrameBase* frame = g_renderBackend->GetCurrentFrame();
            subsystem->CaptureFrame(frame);

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(UpdateScreenCapture, WeakHandle<ScreenCaptureRenderSubsystem>(WeakHandleFromThis()));
}

void ScreenCaptureRenderSubsystem::CaptureFrame(FrameBase* frame)
{
    Threads::AssertOnThread(g_renderThread);

    Assert(m_texture.IsValid());
    Assert(m_texture->IsReady());

    DeferredRenderer* deferredRenderer = static_cast<DeferredRenderer*>(g_renderGlobalState->mainRenderer);
    AssertDebug(deferredRenderer != nullptr);

    DeferredPassData* pd = deferredRenderer->GetLastFrameData().GetPassDataForView(m_view.Get());

    if (!pd)
    {
        HYP_LOG(Rendering, Warning, "No pass data found for view {}. Skipping screen capture.", m_view->Id());

        return;
    }

    const ImageRef& imageRef = deferredRenderer->GetRendererConfig().taaEnabled
        ? pd->temporalAa->GetResultTexture()->GetGpuImage()
        : pd->tonemapPass->GetFinalImageView()->GetImage();

    Assert(imageRef.IsValid());

    // wait for the image to be ready
    if (imageRef->GetResourceState() == RS_UNDEFINED)
    {
        HYP_LOG(Rendering, Warning, "Screen capture image is not ready. Skipping capture.");
        return;
    }

    if (m_texture->GetExtent() != imageRef->GetExtent())
    {
        // HYP_LOG(Rendering, Debug, "Resizing screen capture texture from {} to {}", m_texture->GetExtent(), imageRef->GetExtent());

        // DelegateHandler* delegateHandle = new DelegateHandler();
        // *delegateHandle = frame->OnFrameEnd.Bind([delegateHandle, weakThis = WeakHandleFromThis(), newExtent = imageRef->GetExtent()](...)
        //     {
        //         if (Handle<ScreenCaptureRenderSubsystem> subsystem = weakThis.Lock().Cast<ScreenCaptureRenderSubsystem>())
        //         {
        //             Assert(subsystem->m_texture.IsValid());

        //             subsystem->m_texture->Resize(newExtent);

        //             subsystem->OnTextureResize(subsystem->m_texture);
        //         }

        //         delete delegateHandle;
        //     });
    }

    const ResourceState previousResourceState = imageRef->GetResourceState();

    frame->renderQueue << InsertBarrier(imageRef, RS_COPY_SRC);

    switch (m_screenCaptureMode)
    {
    case ScreenCaptureMode::TO_TEXTURE:
        Assert(m_texture->GetGpuImage()->IsCreated());

        frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_COPY_DST);
        frame->renderQueue << Blit(imageRef, m_texture->GetGpuImage());
        frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_SHADER_RESOURCE);

        break;
    case ScreenCaptureMode::TO_BUFFER:
        Assert(m_buffer.IsValid() && m_buffer->Size() >= imageRef->GetByteSize());

        frame->renderQueue << InsertBarrier(m_buffer, RS_COPY_DST);
        frame->renderQueue << CopyImageToBuffer(imageRef, m_buffer);
        frame->renderQueue << InsertBarrier(m_buffer, RS_COPY_SRC);

        break;
    default:
        HYP_THROW("Invalid screen capture mode");
    }

    frame->renderQueue << InsertBarrier(imageRef, previousResourceState);
}

} // namespace hyperion
