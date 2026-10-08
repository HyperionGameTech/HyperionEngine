/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/Swapchain.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>

namespace Hyperion {

class ApplicationWindow;

HYP_CLASS(NoScriptBindings)
class WebGPUSwapchain final : public SwapchainBase
{
    HYP_OBJECT_BODY(WebGPUSwapchain);

public:
    WebGPUSwapchain(ApplicationWindow* window, const Vec2u& extent);
    ~WebGPUSwapchain() override;

    bool IsCreated() const override;

    RendererResult Create() override;

    void SetExtent(Vec2u newExtent) override;
    void Recreate() override;

    void PrepareForFrame();
    void PresentFrame();

private:
    void Configure();
    void ReleaseCurrentTexture();

    ApplicationWindow* m_window;

    WGPUSurface m_surface;
    WGPUTexture m_currentTexture;
    WGPUTextureView m_currentTextureView;

    bool m_isConfigured;
};

} // namespace Hyperion
