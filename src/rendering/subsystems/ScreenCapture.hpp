/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCREEN_CAPTURE_HPP
#define HYPERION_SCREEN_CAPTURE_HPP

#include <scene/Subsystem.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <rendering/RenderResource.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/Handle.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/functional/Delegate.hpp>

#include <core/object/HypObject.hpp>

#include <Types.hpp>

namespace hyperion {

class View;

enum class ScreenCaptureMode
{
    TO_TEXTURE,
    TO_BUFFER
};

HYP_CLASS(NoScriptBindings)
class HYP_API ScreenCaptureRenderSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(ScreenCaptureRenderSubsystem);

public:
    ScreenCaptureRenderSubsystem(const Handle<View>& view, ScreenCaptureMode screen_capture_mode = ScreenCaptureMode::TO_TEXTURE);
    ScreenCaptureRenderSubsystem(const ScreenCaptureRenderSubsystem& other) = delete;
    ScreenCaptureRenderSubsystem& operator=(const ScreenCaptureRenderSubsystem& other) = delete;
    ScreenCaptureRenderSubsystem(ScreenCaptureRenderSubsystem&& other) noexcept = delete;
    ScreenCaptureRenderSubsystem& operator=(ScreenCaptureRenderSubsystem&& other) noexcept = delete;
    virtual ~ScreenCaptureRenderSubsystem() override;

    HYP_FORCE_INLINE const GpuBufferRef& GetBuffer() const
    {
        return m_buffer;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetTexture() const
    {
        return m_texture;
    }

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;
    virtual void Update(float delta) override;

    Delegate<void, Handle<Texture>> OnTextureResize;

private:
    virtual void Init() override;

    void CaptureFrame(FrameBase* frame);

    Handle<View> m_view;
    ScreenCaptureMode m_screen_capture_mode;
    Handle<Texture> m_texture;
    GpuBufferRef m_buffer;
};

} // namespace hyperion

#endif