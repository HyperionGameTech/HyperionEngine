/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENV_PROBE_HPP
#define HYPERION_ENV_PROBE_HPP

#include <core/Base.hpp>
#include <core/containers/Bitset.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <math/BoundingBox.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <GameCounter.hpp>
#include <HashCode.hpp>

namespace hyperion {

struct RENDER_COMMAND(UpdateEnvProbeDrawProxy);
struct RENDER_COMMAND(CreateCubemapBuffers);
struct RENDER_COMMAND(DestroyCubemapRenderPass);

enum class EnvProbeFlags : uint32
{
    NONE                = 0x0,
    PARALLAX_CORRECTED  = 0x1,
    SHADOW              = 0x2,
    DIRTY               = 0x4,
    MAX                 = 0x7 // 3 bits after are used for shadow
};

HYP_MAKE_ENUM_FLAGS(EnvProbeFlags);

enum EnvProbeBindingSlot : uint32
{
    ENV_PROBE_BINDING_SLOT_INVALID          = uint32(-1),

    ENV_PROBE_BINDING_SLOT_CUBEMAP          = 0,
    ENV_PROBE_BINDING_SLOT_SHADOW_CUBEMAP   = 1,
    ENV_PROBE_BINDING_SLOT_MAX
};

enum EnvProbeType : uint32
{
    ENV_PROBE_TYPE_INVALID      = uint32(-1),

    ENV_PROBE_TYPE_REFLECTION   = 0,
    ENV_PROBE_TYPE_SKY,
    ENV_PROBE_TYPE_SHADOW,

    // These below types are controlled by EnvGrid
    ENV_PROBE_TYPE_AMBIENT,

    ENV_PROBE_TYPE_MAX
};

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

class EnvProbe;

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

struct EnvProbeDrawProxy
{
    ID<EnvProbe>                id;
    BoundingBox                 aabb;
    Vec3f                       world_position;
    uint32                      texture_index;
    float                       camera_near;
    float                       camera_far;
    EnumFlags<EnvProbeFlags>    flags;
    uint32                      grid_slot;
    uint64                      visibility_bits; // bitmask indicating if EnvProbe is visible to cameras by camera ID
};

HYP_CLASS()
class HYP_API EnvProbe : public HypObject<EnvProbe>
{
    HYP_OBJECT_BODY(EnvProbe);

public:
    friend struct RENDER_COMMAND(UpdateEnvProbeDrawProxy);
    friend struct RENDER_COMMAND(DestroyCubemapRenderPass);

    EnvProbe();
    
    EnvProbe(
        const Handle<Scene> &parent_scene,
        const BoundingBox &aabb,
        const Vec2u &dimensions,
        EnvProbeType env_probe_type
    );
    
    EnvProbe(
        const Handle<Scene> &parent_scene,
        const BoundingBox &aabb,
        const Vec2u &dimensions,
        EnvProbeType env_probe_type,
        const ShaderRef &custom_shader
    );

    EnvProbe(const EnvProbe &other)             = delete;
    EnvProbe &operator=(const EnvProbe &other)  = delete;
    ~EnvProbe();
    
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
    
    HYP_FORCE_INLINE const BoundingBox &GetAABB() const
        { return m_aabb; }

    HYP_FORCE_INLINE void SetAABB(const BoundingBox &aabb)
    {
        if (m_aabb != aabb) {
            m_aabb = aabb;

            SetNeedsUpdate(true);
        }
    }
    
    HYP_FORCE_INLINE Vec3f GetOrigin() const
    {
        // ambient probes use the min point of the aabb as the origin,
        // so it can blend between 7 other probes
        if (m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_AMBIENT) {
            return m_aabb.GetMin();
        } else {
            return m_aabb.GetCenter();
        }
    }
    
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

    HYP_FORCE_INLINE const EnvProbeDrawProxy &GetProxy() const
        { return m_proxy; }

    bool IsVisible(ID<Camera> camera_id) const;
    void SetIsVisible(ID<Camera> camera_id, bool is_visible);

    void Init();
    void EnqueueBind() const;
    void EnqueueUnbind() const;
    void Update(GameCounter::TickUnit delta);

    void Render(Frame *frame);

    void UpdateRenderData(bool set_texture = false);

    void UpdateRenderData(
        uint32 texture_slot,
        uint32 grid_slot,
        const Vec3u &grid_size
    );

    void BindToIndex(const EnvProbeIndex &probe_index);

    uint32 m_grid_slot = ~0u; // temp
    
private:
    bool OnlyCollectStaticEntities() const
        { return IsReflectionProbe() || IsAmbientProbe(); }

    void CreateShader();
    void CreateFramebuffer();

    Handle<Scene>           m_parent_scene;
    BoundingBox             m_aabb;
    Vec2u                   m_dimensions;
    EnvProbeType            m_env_probe_type;

    float                   m_camera_near;
    float                   m_camera_far;

    Handle<Texture>         m_texture;
    FramebufferRef          m_framebuffer;
    ShaderRef               m_shader;
    Handle<Camera>          m_camera;
    RenderCollector         m_render_collector;

    EnvProbeIndex           m_bound_index;

    Bitset                  m_visibility_bits;

    bool                    m_needs_update;
    AtomicVar<bool>         m_is_rendered;
    AtomicVar<int32>        m_needs_render_counter;
    HashCode                m_octant_hash_code;

    EnvProbeDrawProxy       m_proxy;
};

} // namespace hyperion

#endif // !HYPERION_ENV_PROBE_HPP
