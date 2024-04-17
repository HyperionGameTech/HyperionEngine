/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/render_components/ScreenCapture.hpp>

#include <Engine.hpp>

namespace hyperion {

ScreenCaptureRenderComponent::ScreenCaptureRenderComponent(Name name, const Extent2D window_size, ScreenCaptureMode screen_capture_mode)
    : RenderComponent(name),
      m_window_size(window_size),
      m_texture(CreateObject<Texture>(Texture2D(
          m_window_size,
          InternalFormat::RGBA8,
          FilterMode::TEXTURE_FILTER_NEAREST,
          WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
          nullptr
      ))),
      m_screen_capture_mode(screen_capture_mode)
{
}

void ScreenCaptureRenderComponent::Init()
{
    InitObject(m_texture);

    m_buffer = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);
    HYPERION_ASSERT_RESULT(m_buffer->Create(g_engine->GetGPUDevice(), m_texture->GetImage()->GetByteSize()));
    m_buffer->SetResourceState(renderer::ResourceState::COPY_DST);
    m_buffer->GetMapping(g_engine->GetGPUDevice());
}

void ScreenCaptureRenderComponent::InitGame()
{
    
}

void ScreenCaptureRenderComponent::OnRemoved()
{
    SafeRelease(std::move(m_buffer));
    g_safe_deleter->SafeReleaseHandle(std::move(m_texture));
}

void ScreenCaptureRenderComponent::OnUpdate(GameCounter::TickUnit delta)
{
    // Do nothing
}

void ScreenCaptureRenderComponent::OnRender(Frame *frame)
{
    auto &deferred_renderer = g_engine->GetDeferredRenderer();

    const FinalPass &final_pass = g_engine->GetFinalPass();
    const ImageRef &image_ref = final_pass.GetLastFrameImage();
    AssertThrow(image_ref.IsValid());
    
    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();

    image_ref->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

    switch (m_screen_capture_mode) {
    case ScreenCaptureMode::TO_TEXTURE:
        m_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        m_texture->GetImage()->Blit(
            command_buffer,
            image_ref
        );

        m_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

        break;
    case ScreenCaptureMode::TO_BUFFER:
        m_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        image_ref->CopyToBuffer(
            command_buffer,
            m_buffer
        );
    
        m_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

        break;
    default:
        HYP_THROW("Invalid screen capture mode");
    }
}

} // namespace hyperion