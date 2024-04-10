#ifndef HYPERION_V2_SCREEN_CAPTURE_HPP
#define HYPERION_V2_SCREEN_CAPTURE_HPP

#include <core/Base.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Light.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/EnvProbe.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <core/lib/FixedArray.hpp>

#include <math/BoundingBox.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>
#include <Types.hpp>

#include <array>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;

class HYP_API ScreenCaptureRenderComponent : public RenderComponent<ScreenCaptureRenderComponent>
{
    Extent2D        m_window_size;
    Handle<Texture> m_texture;
    GPUBufferRef    m_buffer;

public:
    ScreenCaptureRenderComponent(Name name, const Extent2D window_size)
        : RenderComponent(name),
          m_window_size(window_size)
    {
    }

    virtual ~ScreenCaptureRenderComponent() = default;

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
};

} // namespace hyperion::v2

#endif