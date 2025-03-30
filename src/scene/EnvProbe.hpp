/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENV_PROBE_HPP
#define HYPERION_ENV_PROBE_HPP

#include <core/Base.hpp>
#include <core/containers/Bitset.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/math/BoundingBox.hpp>

#include <rendering/RenderTexture.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <GameCounter.hpp>
#include <HashCode.hpp>

namespace hyperion {

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

HYP_ENUM()
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

class EnvProbe;
class EnvProbeRenderResource;

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

HYP_CLASS()
class HYP_API EnvProbe : public HypObject<EnvProbe>
{
    HYP_OBJECT_BODY(EnvProbe);

public:
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

    HYP_FORCE_INLINE EnvProbeRenderResource &GetRenderResource()
        { return *m_render_resource; }
    
    HYP_METHOD()
    EnvProbeType GetEnvProbeType() const
        { return m_env_probe_type; }
    
    HYP_METHOD()
    bool IsReflectionProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_REFLECTION; }
    
    HYP_METHOD()
    bool IsSkyProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_SKY; }
    
    HYP_METHOD()
    bool IsShadowProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_SHADOW; }
    
    HYP_METHOD()
    bool IsAmbientProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_AMBIENT; }
    
    HYP_METHOD()
    bool IsControlledByEnvGrid() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_AMBIENT; }
    
    HYP_FORCE_INLINE const EnvProbeIndex &GetBoundIndex() const
        { return m_bound_index; }

    HYP_FORCE_INLINE void SetBoundIndex(const EnvProbeIndex &bound_index)
        { m_bound_index = bound_index; }
    
    HYP_METHOD()
    const BoundingBox &GetAABB() const
        { return m_aabb; }

    HYP_METHOD()
    void SetAABB(const BoundingBox &aabb);
    
    HYP_METHOD()
    Vec3f GetOrigin() const
    {
        if (IsAmbientProbe()) {
            // ambient probes use the min point of the aabb as the origin,
            // so it can blend between 7 other probes
            return m_aabb.GetMin();
        } else {
            return m_aabb.GetCenter();
        }
    }

    HYP_METHOD()
    void SetOrigin(const Vec3f &origin);
    
    HYP_FORCE_INLINE const Handle<Texture> &GetTexture() const
        { return m_texture; }

    HYP_FORCE_INLINE Vec2u GetDimensions() const
        { return m_dimensions; }

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

    bool IsVisible(ID<Camera> camera_id) const;
    void SetIsVisible(ID<Camera> camera_id, bool is_visible);

    void Init();
    void EnqueueBind() const;
    void EnqueueUnbind() const;
    void Update(GameCounter::TickUnit delta);

    void Render(Frame *frame);

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
    ResourceHandle          m_camera_resource_handle;

    RenderCollector         m_render_collector;

    EnvProbeIndex           m_bound_index;

    Bitset                  m_visibility_bits;

    bool                    m_needs_update;
    AtomicVar<bool>         m_is_rendered;
    AtomicVar<int32>        m_needs_render_counter;
    HashCode                m_octant_hash_code;

    EnvProbeRenderResource *m_render_resource;
};

} // namespace hyperion

#endif // !HYPERION_ENV_PROBE_HPP
