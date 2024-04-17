/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_FINAL_PASS_HPP
#define HYPERION_FINAL_PASS_HPP

#include <Config.hpp>

#include <rendering/DefaultFormats.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <Types.hpp>

namespace hyperion {

class ShaderManagerSystem;

extern ShaderManagerSystem *g_shader_manager;

struct RENDER_COMMAND(SetUITexture);

class FinalPass;

// Performs tonemapping, samples last postfx in chain
class CompositePass : public FullScreenPass
{
public:
    CompositePass();
    CompositePass(const CompositePass &other)               = delete;
    CompositePass &operator=(const CompositePass &other)    = delete;
    virtual ~CompositePass() override;

    void CreateShader();
    virtual void Create() override;
    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;

private:
};

class FinalPass : public FullScreenPass
{
public:
    friend struct RENDER_COMMAND(SetUITexture);

    FinalPass();
    FinalPass(const FinalPass &other)               = delete;
    FinalPass &operator=(const FinalPass &other)    = delete;
    virtual ~FinalPass() override;

    CompositePass &GetCompositePass()
        { return m_composite_pass; }

    const CompositePass &GetCompositePass() const
        { return m_composite_pass; }

    const Handle<Texture> &GetUITexture() const
        { return m_ui_texture; }
    
    void SetUITexture(Handle<Texture> texture);

    const ImageRef &GetLastFrameImage() const
        { return m_last_frame_image; }

    virtual void Create() override;
    virtual void Destroy() override;

    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;

private:
    void RenderUITexture() const;

    CompositePass           m_composite_pass;
    ImageRef                m_last_frame_image;

    // For UI blitting to screen
    Handle<Texture>         m_ui_texture;
    RC<FullScreenPass>      m_render_texture_to_screen_pass;
    uint8                   m_dirty_frame_indices;
};
} // namespace hyperion

#endif