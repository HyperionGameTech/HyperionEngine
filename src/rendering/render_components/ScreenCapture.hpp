/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCREEN_CAPTURE_HPP
#define HYPERION_SCREEN_CAPTURE_HPP

#include <rendering/RenderComponent.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <math/BoundingBox.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/object/HypObject.hpp>

#include <Types.hpp>

namespace hyperion {

enum class ScreenCaptureMode
{
    TO_TEXTURE,
    TO_BUFFER
};

HYP_CLASS()
class HYP_API ScreenCaptureRenderComponent : public RenderComponentBase
{
    HYP_OBJECT_BODY(ScreenCaptureRenderComponent);

public:
    ScreenCaptureRenderComponent(Name name, const Extent2D window_size, ScreenCaptureMode screen_capture_mode = ScreenCaptureMode::TO_TEXTURE);
    ScreenCaptureRenderComponent(const ScreenCaptureRenderComponent &other)                 = delete;
    ScreenCaptureRenderComponent &operator=(const ScreenCaptureRenderComponent &other)      = delete;
    ScreenCaptureRenderComponent(ScreenCaptureRenderComponent &&other) noexcept             = delete;
    ScreenCaptureRenderComponent &operator=(ScreenCaptureRenderComponent &&other) noexcept  = delete;
    virtual ~ScreenCaptureRenderComponent() override;

    HYP_FORCE_INLINE const GPUBufferRef &GetBuffer() const
        { return m_buffer; }

    HYP_FORCE_INLINE const Handle<Texture> &GetTexture() const
        { return m_texture; }

private:
    virtual void Init() override;
    virtual void InitGame() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(Frame *frame) override;

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override
        { }

    ScreenCaptureMode   m_screen_capture_mode;
    Extent2D            m_window_size;
    Handle<Texture>     m_texture;
    GPUBufferRef        m_buffer;
};

} // namespace hyperion

#endif