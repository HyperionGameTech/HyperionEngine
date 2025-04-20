/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_ENV_PROBE_HPP
#define HYPERION_RENDER_ENV_PROBE_HPP

#include <core/Handle.hpp>

#include <core/math/Matrix4.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class EnvProbe;
class Texture;
class CameraRenderResource;
class SceneRenderResource;

struct EnvProbeShaderData
{
    Matrix4 face_view_matrices[6];

    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4f   world_position;

    uint32  texture_index;
    uint32  flags;
    float   camera_near;
    float   camera_far;

    Vec2u   dimensions;
    uint64  visibility_bits;
    Vec4i   position_in_grid;

    Vec4f   sh[9];
};

static constexpr uint32 max_env_probes = (32ull * 1024ull * 1024ull) / sizeof(EnvProbeShaderData);

struct EnvProbeIndex
{
    Vec3u   position;
    Vec3u   grid_size;

    // defaults such that GetProbeIndex() == ~0u
    // because (~0u * 0 * 0) + (~0u * 0) + ~0u == ~0u
    EnvProbeIndex()
        : position { ~0u, ~0u, ~0u },
          grid_size { 0, 0, 0 }
    {
    }

    EnvProbeIndex(const Vec3u &position, const Vec3u &grid_size)
        : position(position),
          grid_size(grid_size)
    {
    }

    EnvProbeIndex(const EnvProbeIndex &other)                   = default;
    EnvProbeIndex &operator=(const EnvProbeIndex &other)        = default;
    EnvProbeIndex(EnvProbeIndex &&other) noexcept               = default;
    EnvProbeIndex &operator=(EnvProbeIndex &&other) noexcept    = default;
    ~EnvProbeIndex()                                            = default;

    HYP_FORCE_INLINE uint32 GetProbeIndex() const
    {
        return (position.x * grid_size.y * grid_size.z)
            + (position.y * grid_size.z)
            + position.z;
    }

    HYP_FORCE_INLINE bool operator<(uint32 value) const
        { return GetProbeIndex() < value; }

    HYP_FORCE_INLINE bool operator==(uint32 value) const
        { return GetProbeIndex() == value; }

    HYP_FORCE_INLINE bool operator!=(uint32 value) const
        { return GetProbeIndex() != value; }

    HYP_FORCE_INLINE bool operator<(const EnvProbeIndex &other) const
        { return GetProbeIndex() < other.GetProbeIndex(); }

    HYP_FORCE_INLINE bool operator==(const EnvProbeIndex &other) const
        { return GetProbeIndex() == other.GetProbeIndex(); }

    HYP_FORCE_INLINE bool operator!=(const EnvProbeIndex &other) const
        { return GetProbeIndex() != other.GetProbeIndex(); }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetProbeIndex());

        return hc;
    }
};

enum class EnvProbeConvolveMode : uint8
{
    NONE = 0,
    IRRADIANCE_MAP,
    PREFILTERED_ENV_MAP
};

class EnvProbeRenderResource final : public RenderResourceBase
{
public:
    EnvProbeRenderResource(EnvProbe *env_probe);
    virtual ~EnvProbeRenderResource() override;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE EnvProbe *GetEnvProbe() const
        { return m_env_probe; }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE uint32 GetTextureSlot() const
        { return m_texture_slot; }

    void SetTextureSlot(uint32 texture_slot);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const EnvProbeShaderData &GetBufferData() const
        { return m_buffer_data; }

    void SetBufferData(const EnvProbeShaderData &buffer_data);

    HYP_FORCE_INLINE const TResourceHandle<CameraRenderResource> &GetCameraResourceHandle() const
        { return m_camera_resource_handle; }

    void SetCameraResourceHandle(TResourceHandle<CameraRenderResource> &&camera_resource_handle);

    HYP_FORCE_INLINE const TResourceHandle<SceneRenderResource> &GetSceneResourceHandle() const
        { return m_scene_resource_handle; }

    void SetSceneResourceHandle(TResourceHandle<SceneRenderResource> &&scene_resource_handle);
    
    HYP_FORCE_INLINE RenderCollector &GetRenderCollector()
        { return m_render_collector; }

    HYP_FORCE_INLINE const RenderCollector &GetRenderCollector() const
        { return m_render_collector; }

    HYP_FORCE_INLINE const Handle<Texture> &GetTexture() const
        { return m_texture; }

    HYP_FORCE_INLINE const FramebufferRef &GetFramebuffer() const
        { return m_framebuffer; }

    HYP_FORCE_INLINE const ShaderRef &GetShader() const
        { return m_shader; }

    void UpdateRenderData(bool set_texture = false);
    void UpdateRenderData(
        uint32 texture_slot,
        uint32 grid_slot,
        const Vec3u &grid_size
    );

    void EnqueueBind();
    void EnqueueUnbind();

    void Render(FrameBase *frame);
    
protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;
    
    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;
    
private:
    void CreateShader();
    void CreateFramebuffer();
    void CreateTexture();
    
    void BindToIndex(const EnvProbeIndex &probe_index);
    void UpdateBufferData();
    
    bool ShouldComputePrefilteredEnvMap() const;
    bool ShouldComputeIrradianceMap() const;
    void Convolve(FrameBase *frame, EnvProbeConvolveMode convolve_mode);

    void ComputeSH(FrameBase *frame);

    EnvProbe                                *m_env_probe;

    EnvProbeShaderData                      m_buffer_data;

    uint32                                  m_texture_slot;

    EnvProbeIndex                           m_bound_index;

    RenderCollector                         m_render_collector;

    Handle<Texture>                         m_texture;
    FramebufferRef                          m_framebuffer;
    ShaderRef                               m_shader;

    ImageRef                                m_irradiance_image;
    ImageViewRef                            m_irradiance_image_view;

    ImageRef                                m_prefiltered_image;
    ImageViewRef                            m_prefiltered_image_view;

    TResourceHandle<CameraRenderResource>   m_camera_resource_handle;
    TResourceHandle<SceneRenderResource>    m_scene_resource_handle;
};

} // namespace hyperion

#endif
