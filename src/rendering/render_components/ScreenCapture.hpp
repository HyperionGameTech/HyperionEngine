/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCREEN_CAPTURE_HPP
#define HYPERION_SCREEN_CAPTURE_HPP

#include <rendering/PostFX.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Light.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/EnvProbe.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <math/BoundingBox.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <core/containers/FixedArray.hpp>

#include <Types.hpp>

namespace hyperion {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;

enum class ScreenCaptureMode
{
    TO_TEXTURE,
    TO_BUFFER
};

class HYP_API ScreenCaptureRenderComponent : public RenderComponent<ScreenCaptureRenderComponent>
{
    Extent2D        m_window_size;
    Handle<Texture> m_texture;
    GPUBufferRef    m_buffer;

public:
    ScreenCaptureRenderComponent(Name name, const Extent2D window_size, ScreenCaptureMode screen_capture_mode = ScreenCaptureMode::TO_TEXTURE);
    ScreenCaptureRenderComponent(const ScreenCaptureRenderComponent &other)                 = delete;
    ScreenCaptureRenderComponent &operator=(const ScreenCaptureRenderComponent &other)      = delete;
    ScreenCaptureRenderComponent(ScreenCaptureRenderComponent &&other) noexcept             = delete;
    ScreenCaptureRenderComponent &operator=(ScreenCaptureRenderComponent &&other) noexcept  = delete;
    virtual ~ScreenCaptureRenderComponent() override;

    const GPUBufferRef &GetBuffer() const
        { return m_buffer; }

    const Handle<Texture> &GetTexture() const
        { return m_texture; }

    void Init();
    void InitGame();

    void OnRemoved();
    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override
        { }

    ScreenCaptureMode   m_screen_capture_mode;
};

} // namespace hyperion

#endif