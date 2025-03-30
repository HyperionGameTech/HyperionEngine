/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_ENV_PROBE_HPP
#define HYPERION_RENDER_ENV_PROBE_HPP

#include <core/math/Matrix4.hpp>

#include <rendering/RenderResource.hpp>

#include <Types.hpp>

namespace hyperion {

class EnvProbe;

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
    Vec4i   position_offset;
    Vec4u   _pad5;
};

static_assert(sizeof(EnvProbeShaderData) == 512);

static constexpr uint32 max_env_probes = (32ull * 1024ull * 1024ull) / sizeof(EnvProbeShaderData);

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

    void UpdateRenderData(bool set_texture = false);
    void UpdateRenderData(
        uint32 texture_slot,
        uint32 grid_slot,
        const Vec3u &grid_size
    );

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    EnvProbe            *m_env_probe;

    EnvProbeShaderData  m_buffer_data;

    uint32              m_texture_slot;
};

} // namespace hyperion

#endif
