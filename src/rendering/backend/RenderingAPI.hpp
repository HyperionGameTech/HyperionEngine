/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERING_API_HPP
#define HYPERION_BACKEND_RENDERING_API_HPP

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/Proc.hpp>

#include <core/Defines.hpp>

namespace hyperion {

namespace sys {
class AppContext;
} // namespace sys

using sys::AppContext;

namespace renderer {

class IRenderingAPI;
class IFrame;
class ISwapchain;
class IAsyncCompute;

struct QueryImageCapabilitiesResult
{
    bool    supports_2d = false;
    bool    supports_3d = false;
    bool    supports_cubemap = false;
    bool    supports_array = false;
    bool    supports_mipmaps = false;
    bool    supports_storage = false;
};

class IDescriptorSetManager
{
public:
    virtual ~IDescriptorSetManager() = default;
};

class IRenderingAPI
{
public:
    virtual ~IRenderingAPI() = default;

    virtual RendererResult Initialize(AppContext &app_context) = 0;
    virtual RendererResult Destroy() = 0;

    virtual const IRenderConfig &GetRenderConfig() const = 0;

    // Will be moved to ApplicationWindow
    virtual ISwapchain *GetSwapchain() const = 0;

    virtual IAsyncCompute *GetAsyncCompute() const = 0;

    virtual IFrame *GetCurrentFrame() const = 0;
    virtual IFrame *PrepareNextFrame() = 0;
    virtual void PresentFrame(IFrame *frame) = 0;

    virtual InternalFormat GetDefaultFormat(DefaultImageFormatType type) const = 0;

    virtual bool IsSupportedFormat(InternalFormat format, ImageSupportType support_type) const = 0;
    virtual InternalFormat FindSupportedFormat(Span<InternalFormat> possible_formats, ImageSupportType support_type) const = 0;

    virtual QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc &texture_desc) const = 0;

    virtual Delegate<void, ISwapchain *> &GetOnSwapchainRecreatedDelegate() = 0;
};

} // namespace renderer

using renderer::IRenderingAPI;

} // namespace hyperion

#endif