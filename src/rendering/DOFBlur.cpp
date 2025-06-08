/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DOFBlur.hpp>

#include <rendering/Shader.hpp>

#include <rendering/backend/RendererFrame.hpp>

namespace hyperion {

DOFBlur::DOFBlur(const Vec2u& extent, GBuffer* gbuffer)
    : m_gbuffer(gbuffer),
      m_extent(extent)
{
}

DOFBlur::~DOFBlur() = default;

void DOFBlur::Create()
{
    ShaderRef blur_horizontal_shader = ShaderManager::GetInstance()->GetOrCreate(NAME("DOFBlurDirection"), ShaderProperties({ "DIRECTION_HORIZONTAL" }));
    AssertThrow(blur_horizontal_shader.IsValid());

    m_blur_horizontal_pass = MakeUnique<FullScreenPass>(
        blur_horizontal_shader,
        InternalFormat::RGBA8,
        m_extent,
        m_gbuffer);

    m_blur_horizontal_pass->Create();

    ShaderRef blur_vertical_shader = ShaderManager::GetInstance()->GetOrCreate(NAME("DOFBlurDirection"), ShaderProperties({ "DIRECTION_VERTICAL" }));
    AssertThrow(blur_vertical_shader.IsValid());

    m_blur_vertical_pass = MakeUnique<FullScreenPass>(
        blur_vertical_shader,
        InternalFormat::RGBA8,
        m_extent,
        m_gbuffer);

    m_blur_vertical_pass->Create();

    ShaderRef blur_mix_shader = ShaderManager::GetInstance()->GetOrCreate(NAME("DOFBlurMix"));
    AssertThrow(blur_mix_shader.IsValid());

    m_blur_mix_pass = MakeUnique<FullScreenPass>(
        blur_mix_shader,
        InternalFormat::RGBA8,
        m_extent,
        m_gbuffer);

    m_blur_mix_pass->Create();
}

void DOFBlur::Destroy()
{
    m_blur_horizontal_pass.Reset();
    m_blur_vertical_pass.Reset();
    m_blur_mix_pass.Reset();
}

void DOFBlur::Render(FrameBase* frame, RenderView* view)
{
    struct
    {
        Vec2u dimension;
    } push_constants;

    push_constants.dimension = m_extent;

    const uint32 frame_index = frame->GetFrameIndex();

    FixedArray<FullScreenPass*, 2> directional_passes {
        m_blur_horizontal_pass.Get(),
        m_blur_vertical_pass.Get()
    };

    for (FullScreenPass* pass : directional_passes)
    {
        pass->SetPushConstants(&push_constants, sizeof(push_constants));
        pass->Render(frame, view);
    }

    m_blur_mix_pass->SetPushConstants(&push_constants, sizeof(push_constants));
    m_blur_mix_pass->Render(frame, view);
}

} // namespace hyperion