#include <rendering/render_components/ScreenCapture.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void ScreenCaptureRenderComponent::Init()
{
    m_texture = CreateObject<Texture>(Texture2D(
        m_window_size,
        InternalFormat::RGBA8,
        FilterMode::TEXTURE_FILTER_LINEAR,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

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
}

void ScreenCaptureRenderComponent::OnUpdate(GameCounter::TickUnit delta)
{
    // Do nothing
}

void ScreenCaptureRenderComponent::OnRender(Frame *frame)
{
    auto &deferred_renderer = g_engine->GetDeferredRenderer();

    const FinalPass &final_pass = g_engine->GetFinalPass();
    const ImageRef &image_ref = final_pass.GetLastFrameImage();//deferred_renderer.GetCombinedResult()->GetAttachment()->GetImage();//final_pass.GetAttachments()[0]->GetImage();
    AssertThrow(image_ref.IsValid());
    
    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();

    image_ref->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
    m_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

    image_ref->CopyToBuffer(
        command_buffer,
        m_buffer
    );
    
    m_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
}

} // namespace hyperion::v2