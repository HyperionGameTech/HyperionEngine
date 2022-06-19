#ifndef HYPERION_V2_DEFERRED_H
#define HYPERION_V2_DEFERRED_H

#include "full_screen_pass.h"
#include "post_fx.h"
#include "renderer.h"

#include <rendering/backend/renderer_frame.h>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::CommandBuffer;

class UIPass : public FullScreenPass {
public:
    UIPass();
    UIPass(const UIPass &other) = delete;
    UIPass &operator=(const UIPass &other) = delete;
    ~UIPass() = default;

    void Create(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary);
};

class DeferredPass : public FullScreenPass {
public:
    DeferredPass();
    DeferredPass(const DeferredPass &other) = delete;
    DeferredPass &operator=(const DeferredPass &other) = delete;
    ~DeferredPass();

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);
};

class DeferredRenderer : public Renderer {
public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other) = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other) = delete;
    ~DeferredRenderer();

    DeferredPass &GetPass()                          { return m_pass; }
    const DeferredPass &GetPass() const              { return m_pass; }

    UIPass &GetUIPass()                              { return m_ui_pass; }
    const UIPass &GetUIPass() const                  { return m_ui_pass; }

    PostProcessing &GetPostProcessing()              { return m_post_processing; }
    const PostProcessing &GetPostProcessing() const  { return m_post_processing; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    void RenderOpaqueObjects(Engine *engine, Frame *frame);
    void RenderTranslucentObjects(Engine *engine, Frame *frame);

    DeferredPass   m_pass;
    UIPass         m_ui_pass;
    PostProcessing m_post_processing;
};

} // namespace hyperion::v2

#endif