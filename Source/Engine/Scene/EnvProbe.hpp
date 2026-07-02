/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/HashCode.hpp>

#include <Core/Containers/Bitset.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Threading/AtomicFlag.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Scene/Volume.hpp>

#include <Scene/BakedLighting/SphericalHarmonics.hpp>

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class Texture;
class View;
class Light;
class Camera;
class ProbeVolume;
struct RenderProxyEnvProbe;

ENGINE_API extern Pool* g_scenePool;
using SceneAllocator = AllocatorInstance<Pool, &g_scenePool>;

HYP_ENUM()
enum EnvProbeFlags : uint32
{
    EPF_NONE = 0x0,               //!< @edithide
    EPF_PARALLAX_CORRECTED = 0x1, //!< @title="Parallax Corrected"
    EPF_BAKED = 0x2,              //!< @edithide
    EPF_REALTIME = 0x4,           //!< @title="Real-time"
    EPF_ORIGIN_FROM_CENTER = 0x8, //!< @title="Origin from center"
    EPF_HAS_VISIBILITY = 0x10     //!< @title="Prevent light leaking" @description="This EnvProbe stores distance values to a texture, used to prevent light leaks at the cost of more memory usage and rendering time."
};

HYP_MAKE_ENUM_FLAGS(EnvProbeFlags);

HYP_ENUM()
enum EnvProbeType : uint32
{
    EPT_INVALID = ~0u,

    EPT_SKY = 0,
    EPT_REFLECTION,
    EPT_AMBIENT,

    EPT_MAX
};

HYP_CLASS(AssetBucket = "EnvProbes")
class ENGINE_API EnvProbe : public VolumeBase
{
    HYP_OBJECT_BODY(EnvProbe);

public:
    EnvProbe();
    explicit EnvProbe(EnvProbeType envProbeType);
    EnvProbe(EnvProbeType envProbeType, const BoundingBox& aabb, const Vec2u& dimensions);

    EnvProbe(const EnvProbe& other) = delete;
    EnvProbe& operator=(const EnvProbe& other) = delete;

    ~EnvProbe();

    Result Rename(Name name) override;

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

    HYP_METHOD(Property = "EnvProbeFlags", LoadOrder = 10)
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
            // cannot be realtime if baked
            SetEnvProbeFlags((m_envProbeFlags | EPF_BAKED) & ~EPF_REALTIME);
        }
        else
        {
            SetEnvProbeFlags(m_envProbeFlags & ~EPF_BAKED);
        }
    }

    HYP_METHOD()
    bool IsRealtime() const
    {
        return m_envProbeFlags[EPF_REALTIME];
    }

    HYP_FORCE_INLINE bool ShouldComputePrefilteredEnvMap() const
    {
        if (IsReflectionProbe() || IsSkyProbe())
        {
            return m_dimensions.Volume() > 1;
        }

        return false;
    }

    HYP_FORCE_INLINE bool ShouldComputeSphericalHarmonics() const
    {
        return m_dimensions.Volume() > 1;
    }

    HYP_METHOD()
    Vec3f GetOrigin(bool fromCenter) const;

    HYP_METHOD()
    void SetOrigin(const Vec3f& origin, bool fromCenter);

    HYP_METHOD()
    HYP_FORCE_INLINE Camera* GetCamera() const
    {
        return m_camera;
    }

    HYP_FORCE_INLINE const Handle<View>& GetView(uint8 viewIndex) const
    {
        return m_views[viewIndex];
    }

    HYP_FORCE_INLINE const FramebufferRef& GetViewFramebuffer(uint8 viewFramebufferIndex) const
    {
        return m_framebuffers[viewFramebufferIndex];
    }

    HYP_FORCE_INLINE Vec2u GetDimensions() const
    {
        return m_dimensions;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetPrefilteredEnvMap() const
    {
        return m_texture;
    }

    HYP_METHOD(Property = "BakedTexture")
    const Handle<Texture>& GetBakedTexture() const
    {
        return IsBaked() ? m_texture : Handle<Texture>::Null();
    }

    HYP_METHOD(Property = "BakedTexture", LoadOrder = 1)
    void SetBakedTexture(const Handle<Texture>& texture);

    HYP_METHOD(Property = "VisibilityTexture")
    const Handle<Texture>& GetVisibilityTexture() const
    {
        return m_visibilityTexture;
    }

    HYP_METHOD(Property = "VisibilityTexture", LoadOrder = 1)
    void SetVisibilityTexture(const Handle<Texture>& visibilityTexture);

    HYP_METHOD(Property = "SHData", NoScriptBindings)
    HYP_FORCE_INLINE const SphericalHarmonicsData& GetSphericalHarmonicsData() const
    {
        return m_shData;
    }

    HYP_METHOD(Property = "SHData", NoScriptBindings)
    void SetSphericalHarmonicsData(const SphericalHarmonicsData& shData);

    virtual void Update(float delta) override;

    void UpdateRenderProxy(RenderProxyEnvProbe* proxy);

    AtomicFlag needsRender;

protected:
    virtual void Init() override;

    virtual void OnAttachedToNode(Node* node) override;
    virtual void OnDetachedFromNode(Node* node) override;

    virtual void OnAddedToWorld(World* world) override;
    virtual void OnRemovedFromWorld(World* world) override;

    virtual void OnAddedToScene(Scene* scene) override;
    virtual void OnRemovedFromScene(Scene* scene) override;

    virtual void OnTransformUpdated() override;

    HYP_FORCE_INLINE bool OnlyCollectStaticEntities() const
    {
        return !IsRealtime();
    }

    virtual void Invalidate(bool forceRerender = false);

    void CreateCamera();
    void RemoveCamera();

    void CreateVisibilityTexture();

    void CreateViewData();
    void DestroyViewData();

    void EnqueueViewsUpdate();

    HYP_FIELD(Property = "Dimensions")
    Vec2u m_dimensions;

    HYP_FIELD(Property = "EnvProbeType")
    EnvProbeType m_envProbeType;

    HYP_FIELD(Property = "EnvProbeFlags")
    EnumFlags<EnvProbeFlags> m_envProbeFlags;

    HYP_FIELD(Property = "SHData")
    SphericalHarmonicsData m_shData;

    Camera* m_camera;

    FixedArray<Handle<View>, 6> m_views;
    FixedArray<FramebufferRef, 6> m_framebuffers;

    Map<ObjId<Scene>, HashCode, SceneAllocator> m_cachedOctantHashCodes;

    Handle<Texture> m_texture;
    Handle<Texture> m_visibilityTexture;
};

HYP_CLASS()
class ENGINE_API ReflectionProbe : public EnvProbe
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

#if HYP_EDITOR
    HYP_METHOD(EditorOnly, EditAction = "Bake Cubemap", EditCondition = "IsBaked")
    void BakeCubemap();
#endif
};

HYP_CLASS()
class ENGINE_API SkyProbe : public EnvProbe
{
    HYP_OBJECT_BODY(SkyProbe);

    friend class ReflectionProbePass;

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
        return m_texture;
    }

private:
    void Init() override;
};

HYP_CLASS()
class ENGINE_API IrradianceProbe : public EnvProbe
{
    HYP_OBJECT_BODY(IrradianceProbe);

    friend class ProbeVolume;

public:
    IrradianceProbe()
        : EnvProbe(EPT_AMBIENT)
    {
    }

    IrradianceProbe(const BoundingBox& aabb, const Vec2u& dimensions)
        : EnvProbe(EPT_AMBIENT, aabb, dimensions)
    {
    }

    IrradianceProbe(const IrradianceProbe& other) = delete;
    IrradianceProbe& operator=(const IrradianceProbe& other) = delete;

    ~IrradianceProbe() override = default;

    HYP_FORCE_INLINE bool IsAttachedToProbeVolume() const
    {
        return GetParentVolume() != nullptr;
    }

private:
#if HYP_EDITOR
    HYP_METHOD(EditorOnly, EditAction = "Recompute Irradiance")
    void RecomputeIrradiance()
    {
        Invalidate(true);
    }
#endif // HYP_EDITOR

    void Invalidate(bool forceRerender = false) override;

    ProbeVolume* GetParentVolume() const;
};

} // namespace Hyperion
