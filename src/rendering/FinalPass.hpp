/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>

namespace hyperion {

class ShaderManager;
class FullScreenPass;
class Mesh;
struct RenderSetup;

struct RENDER_COMMAND(SetUILayerImageView);

class FinalPass final
{
public:
    friend struct RENDER_COMMAND(SetUILayerImageView);

    FinalPass(const SwapchainRef& swapchain);
    FinalPass(const FinalPass& other) = delete;
    FinalPass& operator=(const FinalPass& other) = delete;
    ~FinalPass();

    void SetUILayerImageView(const GpuImageViewRef& imageView);

    void Create();
    void Render(FrameBase* frame, const RenderSetup& rs);

private:
    SwapchainRef m_swapchain;
    Vec2u m_extent;
    TextureFormat m_imageFormat;
    Handle<Mesh> m_quadMesh;
    GpuImageViewRef m_uiLayerImageView;
    Handle<FullScreenPass> m_renderTextureToScreenPass;
    uint8 m_dirtyFrameIndices;
};
} // namespace hyperion
