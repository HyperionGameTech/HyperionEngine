/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FINAL_PASS_HPP
#define HYPERION_FINAL_PASS_HPP

#include <Config.hpp>

#include <core/Handle.hpp>

#include <rendering/SafeDeleter.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class ShaderManager;
class FullScreenPass;
class RenderWorld;
class Mesh;

extern ShaderManager* g_shader_manager;

struct RENDER_COMMAND(SetUILayerImageView);

class FinalPass final
{
public:
    friend struct RENDER_COMMAND(SetUILayerImageView);

    FinalPass(const SwapchainRef& swapchain);
    FinalPass(const FinalPass& other) = delete;
    FinalPass& operator=(const FinalPass& other) = delete;
    ~FinalPass();

    void SetUILayerImageView(const ImageViewRef& image_view);

    void Create();
    void Render(FrameBase* frame, RenderWorld* render_world);

private:
    SwapchainRef m_swapchain;
    Vec2u m_extent;
    TextureFormat m_image_format;
    Handle<Mesh> m_quad_mesh;
    ImageViewRef m_ui_layer_image_view;
    UniquePtr<FullScreenPass> m_render_texture_to_screen_pass;
    uint8 m_dirty_frame_indices;
};
} // namespace hyperion

#endif