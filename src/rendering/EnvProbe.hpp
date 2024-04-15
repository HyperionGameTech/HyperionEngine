/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ENV_PROBE_HPP
#define HYPERION_V2_ENV_PROBE_HPP

#include <HashCode.hpp>
#include <core/Base.hpp>
#include <core/lib/Bitset.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/AtomicVar.hpp>
#include <math/BoundingBox.hpp>
#include <rendering/Texture.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>

namespace hyperion::v2 {

struct RenderCommand_UpdateEnvProbeDrawProxy;
struct RenderCommand_CreateCubemapBuffers;
struct RenderCommand_DestroyCubemapRenderPass;

class Framebuffer;

using renderer::Attachment;
using renderer::Image;

enum EnvProbeBindingSlot : uint
{
    ENV_PROBE_BINDING_SLOT_INVALID          = uint(-1),

    ENV_PROBE_BINDING_SLOT_CUBEMAP          = 0,
    ENV_PROBE_BINDING_SLOT_SHADOW_CUBEMAP   = 1,
    ENV_PROBE_BINDING_SLOT_MAX
};

enum EnvProbeType : uint
{
    ENV_PROBE_TYPE_INVALID = uint(-1),

    ENV_PROBE_TYPE_REFLECTION = 0,
    ENV_PROBE_TYPE_SKY,
    ENV_PROBE_TYPE_SHADOW,

    // These below types are controlled by EnvGrid
    ENV_PROBE_TYPE_AMBIENT,

    ENV_PROBE_TYPE_MAX
};

class EnvProbe;

struct RENDER_COMMAND(UpdateEnvProbeDrawProxy) : renderer::RenderCommand
{
    EnvProbe &env_probe;
    EnvProbeDrawProxy draw_proxy;

    RENDER_COMMAND(UpdateEnvProbeDrawProxy)(EnvProbe &env_probe, EnvProbeDrawProxy &&draw_proxy)
        : env_probe(env_probe),
          draw_proxy(std::move(draw_proxy))
    {
    }

    virtual Result operator()() override;
};

struct EnvProbeIndex
{
    Vec3u       position;
    Extent3D    grid_size;

    // defaults such that GetProbeIndex() == ~0u
    // because (~0u * 0 * 0) + (~0u * 0) + ~0u == ~0u
    EnvProbeIndex()
        : position { ~0u, ~0u, ~0u },
          grid_size { 0, 0, 0 }
    {
    }

    EnvProbeIndex(const Vec3u &position, const Extent3D &grid_size)
        : position(position),
          grid_size(grid_size)
    {
    }

    EnvProbeIndex(const EnvProbeIndex &other)                   = default;
    EnvProbeIndex &operator=(const EnvProbeIndex &other)        = default;
    EnvProbeIndex(EnvProbeIndex &&other) noexcept               = default;
    EnvProbeIndex &operator=(EnvProbeIndex &&other) noexcept    = default;
    ~EnvProbeIndex()                                            = default;

    uint GetProbeIndex() const
    {
        return (position.x * grid_size.height * grid_size.depth)
            + (position.y * grid_size.depth)
            + position.z;
    }

    bool operator<(uint value) const
        { return GetProbeIndex() < value; }

    bool operator==(uint value) const
        { return GetProbeIndex() == value; }

    bool operator!=(uint value) const
        { return GetProbeIndex() != value; }

    bool operator<(const EnvProbeIndex &other) const
        { return GetProbeIndex() < other.GetProbeIndex(); }

    bool operator==(const EnvProbeIndex &other) const
        { return GetProbeIndex() == other.GetProbeIndex(); }

    bool operator!=(const EnvProbeIndex &other) const
        { return GetProbeIndex() != other.GetProbeIndex(); }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetProbeIndex());

        return hc;
    }
};

class EnvProbe
    : public BasicObject<STUB_CLASS(EnvProbe)>,
      public HasDrawProxy<STUB_CLASS(EnvProbe)>
{
public:
    friend struct RenderCommand_UpdateEnvProbeDrawProxy;
    friend struct RenderCommand_DestroyCubemapRenderPass;
    
    HYP_API EnvProbe(
        const Handle<Scene> &parent_scene,
        const BoundingBox &aabb,
        const Extent2D &dimensions,
        EnvProbeType env_probe_type
    );
    
    HYP_API EnvProbe(
        const Handle<Scene> &parent_scene,
        const BoundingBox &aabb,
        const Extent2D &dimensions,
        EnvProbeType env_probe_type,
        Handle<Shader> custom_shader
    );

    EnvProbe(const EnvProbe &other)             = delete;
    EnvProbe &operator=(const EnvProbe &other)  = delete;
    HYP_API ~EnvProbe();

    HYP_FORCE_INLINE EnvProbeType GetEnvProbeType() const
        { return m_env_probe_type; }

    HYP_FORCE_INLINE bool IsReflectionProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_REFLECTION; }

    HYP_FORCE_INLINE bool IsSkyProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_SKY; }

    HYP_FORCE_INLINE bool IsShadowProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_SHADOW; }

    HYP_FORCE_INLINE bool IsAmbientProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_AMBIENT; }

    HYP_FORCE_INLINE bool IsControlledByEnvGrid() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_AMBIENT; }

    HYP_FORCE_INLINE const EnvProbeIndex &GetBoundIndex() const
        { return m_bound_index; }

    HYP_FORCE_INLINE void SetBoundIndex(const EnvProbeIndex &bound_index)
        { m_bound_index = bound_index; }

    HYP_FORCE_INLINE const Matrix4 &GetProjectionMatrix() const
        { return m_projection_matrix; }

    HYP_FORCE_INLINE const FixedArray<Matrix4, 6> &GetViewMatrices() const
        { return m_view_matrices; }

    HYP_FORCE_INLINE const BoundingBox &GetAABB() const
        { return m_aabb; }

    HYP_FORCE_INLINE void SetAABB(const BoundingBox &aabb)
    {
        if (m_aabb != aabb) {
            m_aabb = aabb;

            SetNeedsUpdate(true);
        }
    }

    Vec3f GetOrigin() const
    {
        // ambient probes use the min point of the aabb as the origin,
        // so it can blend between 7 other probes
        if (m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_AMBIENT) {
            return m_aabb.GetMin();
        } else {
            return m_aabb.GetCenter();
        }
    }

    HYP_FORCE_INLINE Handle<Texture> &GetTexture()
        { return m_texture; }

    HYP_FORCE_INLINE const Handle<Texture> &GetTexture() const
        { return m_texture; }

    HYP_FORCE_INLINE void SetNeedsUpdate(bool needs_update)
        { m_needs_update = needs_update; }

    HYP_FORCE_INLINE bool NeedsUpdate() const
        { return m_needs_update; }

    HYP_FORCE_INLINE void SetNeedsRender(bool needs_render)
    {
        if (needs_render) {
            m_needs_render_counter.Set(1, MemoryOrder::RELAXED);
        } else {
            m_needs_render_counter.Set(0, MemoryOrder::RELAXED);
        }
    }

    HYP_FORCE_INLINE bool NeedsRender() const
    {
        const int32 counter = m_needs_render_counter.Get(MemoryOrder::RELAXED);

        return counter > 0;
    }

    HYP_API bool IsVisible(ID<Camera> camera_id) const;
    HYP_API void SetIsVisible(ID<Camera> camera_id, bool is_visible);

    HYP_API void Init();
    HYP_API void EnqueueBind() const;
    HYP_API void EnqueueUnbind() const;
    HYP_API void Update(GameCounter::TickUnit delta);

    HYP_API void Render(Frame *frame);

    void UpdateRenderData(bool set_texture = false);

    void UpdateRenderData(
        uint32 texture_slot,
        uint32 grid_slot,
        Extent3D grid_size
    );

    void BindToIndex(const EnvProbeIndex &probe_index);

    uint32 m_grid_slot = ~0u; // temp
    
private:
    bool OnlyCollectStaticEntities() const
        { return IsReflectionProbe() || IsAmbientProbe(); }

    void CreateShader();
    void CreateFramebuffer();

    Handle<Scene>                                       m_parent_scene;
    BoundingBox                                         m_aabb;
    Extent2D                                            m_dimensions;
    EnvProbeType                                        m_env_probe_type;

    float                                               m_camera_near;
    float                                               m_camera_far;

    Handle<Texture>                                     m_texture;
    Handle<Framebuffer>                                 m_framebuffer;
    Handle<Shader>                                      m_shader;
    Handle<Camera>                                      m_camera;
    RenderList                                          m_render_list;

    Matrix4                                             m_projection_matrix;
    FixedArray<Matrix4, 6>                              m_view_matrices;

    EnvProbeIndex                                       m_bound_index;

    Bitset                                              m_visibility_bits;

    bool                                                m_needs_update;
    AtomicVar<bool>                                     m_is_rendered;
    AtomicVar<int32>                                    m_needs_render_counter;
    HashCode                                            m_octant_hash_code;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP
