/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Bitset.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/BoundingBox.hpp>

#include <scene/Entity.hpp>

#include <rendering/RenderCollection.hpp>

#include <rendering/RenderCommand.hpp>

#include <util/GameCounter.hpp>
#include <core/HashCode.hpp>

namespace hyperion {

class Texture;
class View;
class Light;
class RenderProxyEnvProbe;

enum class EnvProbeFlags : uint32
{
    NONE = 0x0,
    PARALLAX_CORRECTED = 0x1,
    SHADOW = 0x2,
    DIRTY = 0x4,
    MAX = 0x7 // 3 bits after are used for shadow
};

HYP_MAKE_ENUM_FLAGS(EnvProbeFlags);

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
    EnvProbe(EnvProbeType envProbeType);
    EnvProbe(EnvProbeType envProbeType, const BoundingBox& aabb, const Vec2u& dimensions);

    EnvProbe(const EnvProbe& other) = delete;
    EnvProbe& operator=(const EnvProbe& other) = delete;
    ~EnvProbe();

    HYP_FORCE_INLINE const Handle<View>& GetView() const
    {
        return m_view;
    }

    HYP_METHOD()
    EnvProbeType GetEnvProbeType() const
    {
        return m_envProbeType;
    }

    HYP_METHOD()
    bool IsReflectionProbe() const
    {
        return m_envProbeType == EPT_REFLECTION;
    }

    HYP_METHOD()
    bool IsSkyProbe() const
    {
        return m_envProbeType == EPT_SKY;
    }

    HYP_METHOD()
    bool IsShadowProbe() const
    {
        return m_envProbeType == EPT_SHADOW;
    }

    HYP_METHOD()
    bool IsAmbientProbe() const
    {
        return m_envProbeType == EPT_AMBIENT;
    }

    HYP_METHOD()
    bool IsControlledByEnvGrid() const
    {
        return m_envProbeType == EPT_AMBIENT;
    }

    HYP_FORCE_INLINE bool ShouldComputePrefilteredEnvMap() const
    {
        if (IsControlledByEnvGrid())
        {
            return false;
        }

        if (IsReflectionProbe() || IsSkyProbe())
        {
            return m_dimensions.Volume() > 1;
        }

        return false;
    }

    HYP_FORCE_INLINE bool ShouldComputeSphericalHarmonics() const
    {
        if (IsControlledByEnvGrid())
        {
            return false;
        }

        if (IsReflectionProbe() || IsSkyProbe())
        {
            return m_dimensions.Volume() > 1;
        }

        return false;
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

    HYP_FORCE_INLINE const Handle<Texture>& GetPrefilteredEnvMap() const
    {
        return m_prefilteredEnvMap;
    }

    HYP_DEPRECATED HYP_FORCE_INLINE void SetNeedsRender(bool needsRender)
    {
        if (needsRender)
        {
            m_needsRenderCounter.Set(1, MemoryOrder::RELAXED);
        }
        else
        {
            m_needsRenderCounter.Set(0, MemoryOrder::RELAXED);
        }
    }

    HYP_DEPRECATED HYP_FORCE_INLINE bool NeedsRender() const
    {
        const int32 counter = m_needsRenderCounter.Get(MemoryOrder::RELAXED);

        return counter > 0;
    }

    HYP_DEPRECATED bool IsVisible(ObjId<Camera> cameraId) const;
    HYP_DEPRECATED void SetIsVisible(ObjId<Camera> cameraId, bool isVisible);

    HYP_FORCE_INLINE const EnvProbeSphericalHarmonics& GetSphericalHarmonicsData() const
    {
        return m_shData;
    }
    
    HYP_FORCE_INLINE void SetSphericalHarmonicsData(const EnvProbeSphericalHarmonics& shData)
    {
        m_shData = shData;
        SetNeedsRenderProxyUpdate();
    }

    virtual void Update(float delta) override;

    void UpdateRenderProxy(RenderProxyEnvProbe* proxy);

    uint32 m_gridSlot = ~0u; // temp
    Vec4i m_positionInGrid; // temp

protected:
    virtual void OnAttachedToNode(Node* node) override;
    virtual void OnDetachedFromNode(Node* node) override;
    
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
        m_octantHashCode = HashCode();
    }

    virtual void Init() override;

    void CreateView();

    Handle<View> m_view;

    HYP_FIELD(Property = "AABB", Serialize = true)
    BoundingBox m_aabb;

    HYP_FIELD(Property = "Dimensions", Serialize = true)
    Vec2u m_dimensions;

    HYP_FIELD(Property = "EnvProbeType", Serialize = true)
    EnvProbeType m_envProbeType;

    HYP_FIELD(Property = "SHData", Serialize = true)
    EnvProbeSphericalHarmonics m_shData;

    float m_cameraNear;
    float m_cameraFar;

    Handle<Camera> m_camera;

    Bitset m_visibilityBits;

    bool m_needsUpdate;
    AtomicVar<int32> m_needsRenderCounter;
    HashCode m_octantHashCode;

    Handle<Texture> m_prefilteredEnvMap;
};

HYP_CLASS()
class HYP_API ReflectionProbe : public EnvProbe
{
    HYP_OBJECT_BODY(ReflectionProbe);

public:
    ReflectionProbe()
        : EnvProbe(EPT_REFLECTION)
    {
    }

    ReflectionProbe(const BoundingBox& aabb, const Vec2u& dimensions)
        : EnvProbe(EPT_REFLECTION, aabb, dimensions)
    {
    }

    ReflectionProbe(const ReflectionProbe& other) = delete;
    ReflectionProbe& operator=(const ReflectionProbe& other) = delete;
    ~ReflectionProbe() override = default;
};

HYP_CLASS()
class HYP_API SkyProbe : public EnvProbe
{
    HYP_OBJECT_BODY(SkyProbe);

    friend class ReflectionProbeRenderer;

public:
    SkyProbe()
        : EnvProbe(EPT_SKY, BoundingBox(Vec3f(-100.0f), Vec3f(100.0f)), Vec2u(1, 1))
    {
    }

    SkyProbe(const BoundingBox& aabb, const Vec2u& dimensions)
        : EnvProbe(EPT_SKY, aabb, dimensions)
    {
    }

    SkyProbe(const SkyProbe& other) = delete;
    SkyProbe& operator=(const SkyProbe& other) = delete;
    ~SkyProbe() override = default;

    HYP_METHOD()
    const Handle<Texture>& GetSkyboxCubemap() const
    {
        return m_skyboxCubemap;
    }

private:
    void Init() override;

    Handle<Texture> m_skyboxCubemap;
};

} // namespace hyperion

