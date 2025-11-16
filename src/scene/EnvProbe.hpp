/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/HashCode.hpp>

#include <core/containers/Bitset.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/math/BoundingBox.hpp>

#include <scene/Entity.hpp>

#include <rendering/RenderCommand.hpp>

namespace hyperion {

class Texture;
class View;
class Light;
class Camera;
class RenderProxyEnvProbe;

HYP_ENUM()
enum EnvProbeFlags : uint32
{
    EPF_NONE = 0x0,
    EPF_PARALLAX_CORRECTED = 0x1,
    EPF_BAKED = 0x2
};

HYP_MAKE_ENUM_FLAGS(EnvProbeFlags);

HYP_ENUM()
enum EnvProbeType : uint32
{
    EPT_INVALID = ~0u,

    EPT_SKY = 0,
    EPT_REFLECTION,

    // These below types are controlled by EnvGrid
    EPT_AMBIENT,

    EPT_MAX
};

HYP_STRUCT(Serialize = "bitwise")
struct EnvProbeSphericalHarmonics
{
    HYP_STRUCT_BODY(EnvProbeSphericalHarmonics);

    Vec4f values[9];
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

    HYP_METHOD(Property = "EnvProbeFlags")
    EnumFlags<EnvProbeFlags> GetEnvProbeFlags() const
    {
        return m_envProbeFlags;
    }

    HYP_METHOD(Property = "EnvProbeFlags")
    void SetEnvProbeFlags(EnumFlags<EnvProbeFlags> envProbeFlags);

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
    bool IsAmbientProbe() const
    {
        return m_envProbeType == EPT_AMBIENT;
    }

    HYP_METHOD()
    bool IsBaked() const
    {
        return m_envProbeFlags[EPF_BAKED];
    }

    HYP_METHOD()
    void SetIsBaked(bool isBaked)
    {
        if (isBaked)
        {
            SetEnvProbeFlags(m_envProbeFlags | EPF_BAKED);
        }
        else
        {
            SetEnvProbeFlags(m_envProbeFlags & ~EPF_BAKED);
        }
    }

    HYP_FORCE_INLINE bool ShouldComputePrefilteredEnvMap() const
    {
        if (IsBaked())
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
        if (IsBaked())
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

    HYP_METHOD(Property = "BakedTexture")
    const Handle<Texture>& GetBakedTexture() const
    {
        return IsBaked() ? m_prefilteredEnvMap : Handle<Texture>::Null();
    }

    HYP_METHOD(Property = "BakedTexture", LoadOrder = 1)
    void SetBakedTexture(const Handle<Texture>& texture);

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
    Vec4i m_positionInGrid;  // temp

protected:
    virtual void OnAttachedToNode(Node* node) override;
    virtual void OnDetachedFromNode(Node* node) override;

    virtual void OnAddedToWorld(World* world) override;
    virtual void OnRemovedFromWorld(World* world) override;

    virtual void OnAddedToScene(Scene* scene) override;
    virtual void OnRemovedFromScene(Scene* scene) override;

    virtual void OnTransformUpdated(const Transform& transform) override;

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

    HYP_FIELD(Property = "AABB")
    BoundingBox m_aabb;

    HYP_FIELD(Property = "Dimensions")
    Vec2u m_dimensions;

    HYP_FIELD(Property = "EnvProbeType")
    EnvProbeType m_envProbeType;

    HYP_FIELD(Property = "EnvProbeFlags")
    EnumFlags<EnvProbeFlags> m_envProbeFlags;

    HYP_FIELD(Property = "SHData")
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
