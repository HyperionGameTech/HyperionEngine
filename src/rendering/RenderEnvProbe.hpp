/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_ENV_PROBE_HPP
#define HYPERION_RENDER_ENV_PROBE_HPP

#include <core/math/Matrix4.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class EnvProbe;

struct alignas(256) EnvProbeShaderData
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
    Vec2u   _pad2;

    Vec4i   position_in_grid;
    Vec4i   position_offset;
    Vec4u   _pad5;
};

static_assert(sizeof(EnvProbeShaderData) == 512);

static constexpr uint32 max_env_probes = (8ull * 1024ull * 1024ull) / sizeof(EnvProbeShaderData);

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

class EnvProbeRenderResource final : public RenderResourceBase
{
public:
    EnvProbeRenderResource(EnvProbe *env_probe, const ResourceHandle &camera_resource_handle);
    virtual ~EnvProbeRenderResource() override;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE EnvProbe *GetEnvProbe() const
        { return m_env_probe; }

    HYP_FORCE_INLINE RenderCollector &GetRenderCollector()
        { return m_render_collector; }

    HYP_FORCE_INLINE const RenderCollector &GetRenderCollector() const
        { return m_render_collector; }

    HYP_FORCE_INLINE const EnvProbeShaderData &GetBufferData() const
        { return m_buffer_data; }

    void SetBufferData(const EnvProbeShaderData &buffer_data);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE uint32 GetTextureSlot() const
        { return m_texture_slot; }

    void SetTextureSlot(uint32 texture_slot);

    void BindToIndex(const EnvProbeIndex &probe_index);

    void Render(Frame *frame);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    EnvProbe            *m_env_probe;
    ResourceHandle      m_camera_resource_handle;

    EnvProbeShaderData  m_buffer_data;

    RenderCollector     m_render_collector;

    EnvProbeIndex       m_probe_index;
    uint32              m_texture_slot;
    bool                m_needs_render;
};

} // namespace hyperion

#endif
