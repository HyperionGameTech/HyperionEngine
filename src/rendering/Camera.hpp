/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_CAMERA_HPP
#define HYPERION_RENDERING_CAMERA_HPP

#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>

#include <rendering/RenderResources.hpp>

#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class Camera;

struct alignas(256) CameraShaderData
{
    Matrix4     view;
    Matrix4     projection;
    Matrix4     previous_view;

    Vec4u       dimensions;
    Vec4f       camera_position;
    Vec4f       camera_direction;
    Vec4f       jitter;
    
    float       camera_near;
    float       camera_far;
    float       camera_fov;
    uint32      id;
};

static_assert(sizeof(CameraShaderData) == 512);

static constexpr uint32 max_cameras = (16ull * 1024ull) / sizeof(CameraShaderData);

class CameraRenderResources final : public RenderResourcesBase
{
public:
    CameraRenderResources(const WeakHandle<Camera> &camera_weak);
    CameraRenderResources(CameraRenderResources &&other) noexcept;
    virtual ~CameraRenderResources() override;

    HYP_FORCE_INLINE const WeakHandle<Camera> &GetCamera() const
        { return m_camera_weak; }

    void SetBufferData(const CameraShaderData &buffer_data);

    /*! \note This method is only safe to call from the render thread. */
    HYP_FORCE_INLINE const CameraShaderData &GetBufferData() const
        { return m_buffer_data; }

    void ApplyJitter();

protected:
    virtual void Initialize() override;
    virtual void Destroy() override;
    virtual void Update() override;

    virtual uint32 AcquireBufferIndex() const override;
    virtual void ReleaseBufferIndex(uint32 buffer_index) const override;

private:
    void UpdateBufferData();

    WeakHandle<Camera>  m_camera_weak;
    CameraShaderData    m_buffer_data;
};

} // namespace hyperion

#endif
