/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/HashCode.hpp>

#include <Core/containers/Bitset.hpp>

#include <Core/memory/pool/Pool.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <Core/math/BoundingBox.hpp>

#include <scene/Volume.hpp>

#include <rendering/RenderTypes.hpp>

#include <cstring>

namespace Hyperion {

class Texture;
class View;
class Light;
class Camera;
class RenderProxyEnvProbe;

HYP_API extern Pool* g_scenePool;
using SceneAllocator = AllocatorInstance<Pool, &g_scenePool>;

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
    EPT_AMBIENT,

    EPT_MAX
};

#pragma pack(push, 1)

HYP_STRUCT()
struct EnvProbeSphericalHarmonics
{
    HYP_STRUCT_BODY(EnvProbeSphericalHarmonics);

    float values[9 * 3];

    bool operator==(const EnvProbeSphericalHarmonics& other) const
    {
        return std::memcmp(values, other.values, sizeof(values)) == 0;
    }

    bool operator!=(const EnvProbeSphericalHarmonics& other) const
    {
        return std::memcmp(values, other.values, sizeof(values)) != 0;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(
            reinterpret_cast<const ubyte*>(values),
            reinterpret_cast<const ubyte*>(values) + sizeof(values));
    }

#pragma region Serialization

    HYP_METHOD(Property = "Order0", Serialize = true, NoScriptBindings)
    Vec3f GetOrder0() const
    {
        return Vec3f(values[0], values[1], values[2]);
    }

    HYP_METHOD(Property = "Order0", Serialize = true, NoScriptBindings)
    void SetOrder0(const Vec3f& inValues)
    {
        std::memcpy(values, &inValues, sizeof(float) * 3);
    }

    HYP_METHOD(Property = "Order1", Serialize = true, NoScriptBindings)
    FixedArray<Vec3f, 3> GetOrder1() const
    {
        return {
            Vec3f(values[3], values[4], values[5]),
            Vec3f(values[6], values[7], values[8]),
            Vec3f(values[9], values[10], values[11])
        };
    }

    HYP_METHOD(Property = "Order1", Serialize = true, NoScriptBindings)
    void SetOrder1(const FixedArray<Vec3f, 3>& inValues)
    {
        std::memcpy(values + 3, inValues.Data(), sizeof(float) * 9);
    }

    HYP_METHOD(Property = "Order2", Serialize = true, NoScriptBindings)
    FixedArray<Vec3f, 5> GetOrder2() const
    {
        return {
            Vec3f(values[12], values[13], values[14]),
            Vec3f(values[15], values[16], values[17]),
            Vec3f(values[18], values[19], values[20]),
            Vec3f(values[21], values[22], values[23]),
            Vec3f(values[24], values[25], values[26])
        };
    }

    HYP_METHOD(Property = "Order2", Serialize = true, NoScriptBindings)
    void SetOrder2(const FixedArray<Vec3f, 5>& inValues)
    {
        std::memcpy(values + 12, inValues.Data(), sizeof(float) * 15);
    }

#pragma endregion Serialization
};

#pragma pack(pop)

HYP_CLASS(AssetBucket = "EnvProbes")
class HYP_API EnvProbe : public VolumeBase
{
    HYP_OBJECT_BODY(EnvProbe);

public:
    EnvProbe();
    explicit EnvProbe(EnvProbeType envProbeType);
    EnvProbe(EnvProbeType envProbeType, const BoundingBox& aabb, const Vec2u& dimensions);

    EnvProbe(const EnvProbe& other) = delete;
    EnvProbe& operator=(const EnvProbe& other) = delete;
    ~EnvProbe();

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

    HYP_METHOD()
    bool IsRealtime() const
    {
        return !IsBaked();
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
    Vec3f GetOrigin() const
    {
        if (IsAmbientProbe())
        {
            // ambient probes use the min point of the aabb as the origin,
            // so it can blend between 7 other probes
            return GetWorldBounds().GetMin();
        }
        else
        {
            return GetWorldBounds().GetCenter();
        }
    }

    HYP_METHOD()
    void SetOrigin(const Vec3f& origin);

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

    HYP_DEPRECATED bool IsVisible(ObjId<Camera> cameraId) const;
    HYP_DEPRECATED void SetIsVisible(ObjId<Camera> cameraId, bool isVisible);

    HYP_FORCE_INLINE const EnvProbeSphericalHarmonics& GetSphericalHarmonicsData() const
    {
        return m_shData;
    }

    HYP_FORCE_INLINE void SetSphericalHarmonicsData(const EnvProbeSphericalHarmonics& shData)
    {
        m_shData = shData;

        MarkDirty();

        SetNeedsRenderProxyUpdate();
    }

    virtual void Update(float delta) override;

    void UpdateRenderProxy(RenderProxyEnvProbe* proxy);

protected:
    virtual void OnAttachedToNode(Node* node) override;
    virtual void OnDetachedFromNode(Node* node) override;

    virtual void OnAddedToWorld(World* world) override;
    virtual void OnRemovedFromWorld(World* world) override;

    virtual void OnAddedToScene(Scene* scene) override;
    virtual void OnRemovedFromScene(Scene* scene) override;

    virtual void OnTransformUpdated() override;

    HYP_FORCE_INLINE bool OnlyCollectStaticEntities() const
    {
        return IsReflectionProbe() || IsSkyProbe() || IsAmbientProbe();
    }

    HYP_FORCE_INLINE void Invalidate()
    {
        m_cachedOctantHashCodes.Clear();
    }

    virtual void Init() override;

    void CreateViews();

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

    Camera* m_camera;

    FixedArray<Handle<View>, 6> m_views;
    FixedArray<FramebufferRef, 6> m_framebuffers;

    Bitset m_visibilityBits;

    TMap<ObjId<Scene>, HashCode, SceneAllocator> m_cachedOctantHashCodes;

    Handle<Texture> m_texture;
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

#if HYP_EDITOR
    HYP_METHOD(EditorOnly, EditAction = "Bake Cubemap", EditCondition = "IsBaked")
    void BakeCubemap();
#endif
};

HYP_CLASS()
class HYP_API SkyProbe : public EnvProbe
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

} // namespace Hyperion
