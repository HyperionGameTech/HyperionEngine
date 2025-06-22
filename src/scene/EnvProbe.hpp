/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENV_PROBE_HPP
#define HYPERION_ENV_PROBE_HPP

#include <core/Base.hpp>
#include <core/containers/Bitset.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/math/BoundingBox.hpp>

#include <scene/Entity.hpp>

#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <GameCounter.hpp>
#include <HashCode.hpp>

namespace hyperion {

class Texture;
class View;

enum class EnvProbeFlags : uint32
{
    NONE = 0x0,
    PARALLAX_CORRECTED = 0x1,
    SHADOW = 0x2,
    DIRTY = 0x4,
    MAX = 0x7 // 3 bits after are used for shadow
};

HYP_MAKE_ENUM_FLAGS(EnvProbeFlags);

enum EnvProbeBindingSlot : uint32
{
    ENV_PROBE_BINDING_SLOT_INVALID = uint32(-1),

    ENV_PROBE_BINDING_SLOT_CUBEMAP = 0,
    ENV_PROBE_BINDING_SLOT_SHADOW_CUBEMAP = 1,
    ENV_PROBE_BINDING_SLOT_MAX
};

HYP_ENUM()
enum EnvProbeType : uint32
{
    EPT_INVALID = uint32(-1),

    EPT_SKY = 0,
    EPT_REFLECTION,

    EPT_SHADOW,

    // These below types are controlled by EnvGrid
    EPT_AMBIENT,

    EPT_MAX
};

class EnvProbe;
class RenderEnvProbe;

/*! \brief An EnvProbe handles rendering of reflection probes, sky probes, shadow probes, and ambient probes.
 *  \details It is used to capture the environment around a point in space and store it in a cubemap texture.
 *  It can also be used to capture shadows from a light source.
 *  An EnvProbe may be controlled by an EnvGrid in the case of ambient probes, in order to reduce per-probe allocation overhead by batching them together. */
HYP_CLASS()
class HYP_API EnvProbe : public Entity
{
    HYP_OBJECT_BODY(EnvProbe);

public:
    EnvProbe();
    EnvProbe(const BoundingBox& aabb, const Vec2u& dimensions, EnvProbeType env_probe_type);

    EnvProbe(const EnvProbe& other) = delete;
    EnvProbe& operator=(const EnvProbe& other) = delete;
    ~EnvProbe();

    HYP_FORCE_INLINE RenderEnvProbe& GetRenderResource() const
    {
        return *m_render_resource;
    }

    HYP_FORCE_INLINE const Handle<View>& GetView() const
    {
        return m_view;
    }

    HYP_METHOD()
    EnvProbeType GetEnvProbeType() const
    {
        return m_env_probe_type;
    }

    HYP_METHOD()
    bool IsReflectionProbe() const
    {
        return m_env_probe_type == EPT_REFLECTION;
    }

    HYP_METHOD()
    bool IsSkyProbe() const
    {
        return m_env_probe_type == EPT_SKY;
    }

    HYP_METHOD()
    bool IsShadowProbe() const
    {
        return m_env_probe_type == EPT_SHADOW;
    }

    HYP_METHOD()
    bool IsAmbientProbe() const
    {
        return m_env_probe_type == EPT_AMBIENT;
    }

    HYP_METHOD()
    bool IsControlledByEnvGrid() const
    {
        return m_env_probe_type == EPT_AMBIENT;
    }

    HYP_METHOD()
    const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    HYP_METHOD()
    void SetAABB(const BoundingBox& aabb);

    HYP_METHOD()
    Vec3f GetOrigin() const
    {
        if (IsAmbientProbe())
        {
            // ambient probes use the min point of the aabb as the origin,
            // so it can blend between 7 other probes
            return m_aabb.GetMin();
        }
        else
        {
            return m_aabb.GetCenter();
        }
    }

    HYP_METHOD()
    void SetOrigin(const Vec3f& origin);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    HYP_FORCE_INLINE Vec2u GetDimensions() const
    {
        return m_dimensions;
    }

    HYP_DEPRECATED HYP_FORCE_INLINE void SetNeedsRender(bool needs_render)
    {
        if (needs_render)
        {
            m_needs_render_counter.Set(1, MemoryOrder::RELAXED);
        }
        else
        {
            m_needs_render_counter.Set(0, MemoryOrder::RELAXED);
        }
    }

    HYP_DEPRECATED HYP_FORCE_INLINE bool NeedsRender() const
    {
        const int32 counter = m_needs_render_counter.Get(MemoryOrder::RELAXED);

        return counter > 0;
    }

    HYP_DEPRECATED bool IsVisible(ID<Camera> camera_id) const;
    HYP_DEPRECATED void SetIsVisible(ID<Camera> camera_id, bool is_visible);

    virtual void Update(float delta) override;

    uint32 m_grid_slot = ~0u; // temp

protected:
    virtual void OnAddedToWorld(World* world) override;
    virtual void OnRemovedFromWorld(World* world) override;
    virtual void OnAddedToScene(Scene* scene) override;
    virtual void OnRemovedFromScene(Scene* scene) override;

    HYP_FORCE_INLINE bool OnlyCollectStaticEntities() const
    {
        return IsReflectionProbe() || IsSkyProbe() || IsAmbientProbe();
    }

    HYP_FORCE_INLINE void Invalidate()
    {
        m_octant_hash_code = HashCode();
    }

    void Init() override;

    void CreateView();

    Handle<View> m_view;

    HYP_FIELD(Property = "AABB", Serialize = true)
    BoundingBox m_aabb;

    HYP_FIELD(Property = "Dimensions", Serialize = true)
    Vec2u m_dimensions;

    HYP_FIELD(Property = "EnvProbeType", Serialize = true)
    EnvProbeType m_env_probe_type;

    float m_camera_near;
    float m_camera_far;

    Handle<Camera> m_camera;

    Bitset m_visibility_bits;

    bool m_needs_update;
    AtomicVar<int32> m_needs_render_counter;
    HashCode m_octant_hash_code;

    RenderEnvProbe* m_render_resource;
};

} // namespace hyperion

#endif // !HYPERION_ENV_PROBE_HPP
