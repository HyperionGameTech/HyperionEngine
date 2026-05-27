/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>

#include <Core/utilities/DataMutationState.hpp>

#include <Core/math/Color.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/MathUtil.hpp>
#include <Core/math/BoundingBox.hpp>
#include <Core/math/BoundingSphere.hpp>

#include <scene/Entity.hpp>

namespace Hyperion {

class Camera;
class MaterialInstance;
class Texture;
class View;
class RenderProxyLight;

enum ShadowMapFilter : uint32;

HYP_ENUM()
enum class LightType : uint32
{
    Directional = 0,
    Point,
    Spot,
    AreaRect,

    Max
};

static constexpr LightType InvalidLightType = LightType(~0u);
static constexpr uint32 NumLightTypes = uint32(LightType::Max);

HYP_ENUM()
enum class LightFlags : uint32
{
    None = 0x0,

    ShadowCaster = 0x1,

    ShadowPCF = 0x2,
    ShadowContactHardening = 0x4,
    ShadowVariance = 0x8,
    ShadowFilterMask = (ShadowPCF | ShadowContactHardening | ShadowVariance),

    CacheStaticShadowMaps = 0x10,
    BakeStaticShadows = 0x20,

    Default = ShadowCaster | ShadowPCF
};

HYP_MAKE_ENUM_FLAGS(LightFlags);

HYP_CLASS(AssetBucket = "Lights")
class HYP_API Light : public Entity
{
    HYP_OBJECT_BODY(Light);

public:
    Light();

    Light(
        LightType type,
        const Vec3f& position,
        const Color& color,
        float intensity,
        float radius);

    Light(
        LightType type,
        const Vec3f& position,
        const Vec3f& normal,
        const Vec2f& areaSize,
        const Color& color,
        float intensity,
        float radius);

    Light(const Light& other) = delete;
    Light& operator=(const Light& other) = delete;

    Light(Light&& other) noexcept = delete;
    Light& operator=(Light&& other) noexcept = delete;

    virtual ~Light() override;

    /*! \brief Get the type of the light.
     *
     *  \return The type.
     */
    HYP_METHOD()
    LightType GetLightType() const
    {
        return m_type;
    }

    HYP_METHOD()
    EnumFlags<LightFlags> GetLightFlags() const
    {
        return m_lightFlags;
    }

    HYP_METHOD()
    void SetLightFlags(EnumFlags<LightFlags> flags);

    /*! \brief Get the normal for the light. This is used only for area lights.
     *
     *  \return The normal. */
    HYP_METHOD(Property = "Normal", Editor = true)
    const Vec3f& GetNormal() const
    {
        return m_normal;
    }

    /*! \brief Set the normal for the light. This is used only for area lights.
     *
     *  \param normal The normal to set. */
    HYP_METHOD(Property = "Normal", Editor = true)
    void SetNormal(const Vec3f& normal);

    /*! \brief Get the area size for the light. This is used only for area lights.
     *
     *  \return The area size. (x = width, y = height) */
    HYP_METHOD(Property = "AreaSize", Editor = true)
    const Vec2f& GetAreaSize() const
    {
        return m_areaSize;
    }

    /*! \brief Set the area size for the light. This is used only for area lights.
     *
     *  \param areaSize The area size to set. (x = width, y = height) */
    HYP_METHOD(Property = "AreaSize", Editor = true)
    void SetAreaSize(const Vec2f& areaSize);

    HYP_METHOD(Property = "Color", Editor = true)
    const Color& GetColor() const
    {
        return m_color;
    }

    HYP_METHOD(Property = "Color", Editor = true)
    void SetColor(const Color& color);

    HYP_METHOD(Property = "Intensity", Editor = true)
    float GetIntensity() const
    {
        return m_intensity;
    }

    HYP_METHOD(Property = "Intensity", Editor = true)
    void SetIntensity(float intensity);

    HYP_METHOD(Property = "Radius", Editor = true)
    float GetRadius() const
    {
        switch (m_type)
        {
        case LightType::Directional:
            return INFINITY;
        case LightType::Point:
            return m_radius;
        default:
            return 0.0f;
        }
    }

    HYP_METHOD(Property = "Radius", Editor = true)
    void SetRadius(float radius);

    HYP_METHOD(Property = "Falloff", Editor = true)
    float GetFalloff() const
    {
        return m_falloff;
    }

    HYP_METHOD(Property = "Falloff", Editor = true)
    void SetFalloff(float falloff);

    /*! \brief Get the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     *  \return The spotlight angles. */
    HYP_METHOD(Property = "SpotAngles", Editor = true)
    const Vec2f& GetSpotAngles() const
    {
        return m_spotAngles;
    }

    /*! \brief Set the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     *  \param spotAngles The angles to set for the spotlight. */
    HYP_METHOD(Property = "SpotAngles", Editor = true)
    void SetSpotAngles(const Vec2f& spotAngles);

    /*! \brief Get the material  for the light. Used for area lights.
     *
     *  \return The material handle associated with the Light. */
    HYP_METHOD(Property = "Material", Editor = true)
    const Handle<MaterialInstance>& GetMaterial() const
    {
        return m_material;
    }
    /*! \brief Sets the material handle associated with the Light. Used for textured area lights.
     *
     *  \param material The material to set for this Light. */
    HYP_METHOD(Property = "Material", Editor = true)
    void SetMaterial(Handle<MaterialInstance> material);

    HYP_METHOD(Property = "ShadowMapDimensions", Editor = true)
    const Vec2u& GetShadowMapDimensions() const
    {
        return m_shadowMapDimensions;
    }

    HYP_METHOD(Property = "ShadowMapDimensions", Editor = true)
    void SetShadowMapDimensions(Vec2u shadowMapDimensions);

    HYP_METHOD(Property = "ShadowMapCascades", Editor = true)
    uint32 GetNumShadowMapCascades() const
    {
        return m_numShadowMapCascades;
    }

    HYP_METHOD(Property = "ShadowMapCascades", Editor = true)
    void SetNumShadowMapCascades(uint32 numShadowMapCascades);

    /*! \brief Get the baked shadow map for this light - only present if the light has static shadows that have been baked.
     *
     *  \return The baked shadow map, or an empty handle if there is no baked shadow map. */
    HYP_METHOD(Property = "BakedShadowMap")
    HYP_FORCE_INLINE const Handle<Texture>& GetBakedShadowMap() const
    {
        return m_shadowMap;
    }

    HYP_METHOD(Property = "BakedShadowMap")
    void SetBakedShadowMap(const Handle<Texture>& shadowMap);

    HYP_METHOD(Property = "ShadowMapFilter", Editor = true, Transient)
    ShadowMapFilter GetShadowMapFilter() const
    {
        return (ShadowMapFilter)((m_lightFlags & LightFlags::ShadowFilterMask)
                ? MathUtil::FastLog2(m_lightFlags & LightFlags::ShadowFilterMask)
                : 0);
    }

    HYP_METHOD(Property = "ShadowMapFilter", Editor = true, Transient)
    void SetShadowMapFilter(ShadowMapFilter shadowMapFilter);

    BoundingSphere GetBoundingSphere(bool worldSpace) const;

    virtual void SetLocalBounds(const BoundingBox& localBounds) override;

    void UpdateRenderProxy(RenderProxyLight* proxy);

#if HYP_EDITOR
    HYP_METHOD(EditorOnly, EditAction = "Bake shadows for static objects", EditCondition = "CanBakeStaticShadows")
    void BakeStaticShadows();

    HYP_METHOD(EditorOnly, EditAction = "Remove baked shadows", EditCondition = "CanBakeStaticShadows")
    void RemoveBakedShadows()
    {
        SetBakedShadowMap(Handle<Texture>::Null());
    }
#endif

protected:
    void Init() override;
    void Update(float delta) override;

    void OnAttachedToNode(Node* node) override;
    void OnDetachedFromNode(Node* node) override;

    void OnAddedToScene(Scene* scene) override;
    void OnRemovedFromScene(Scene* scene) override;

    void OnTransformUpdated() override;

    BoundingBox CalculateLightBounds() const;

#if HYP_EDITOR
    HYP_METHOD(EditorOnly)
    bool CanBakeStaticShadows() const;
#else
    static constexpr NoOpFunction<bool> CanBakeStaticShadows;
#endif

    HYP_FIELD()
    LightType m_type;

    HYP_FIELD(Property = "LightFlags")
    EnumFlags<LightFlags> m_lightFlags;

    Vec3f m_normal;
    Vec2f m_areaSize;
    Color m_color;
    float m_intensity;
    float m_radius;
    float m_falloff;
    Vec2f m_spotAngles;
    Handle<MaterialInstance> m_material;

    // Only present if baked
    Handle<Texture> m_shadowMap;

    HYP_FIELD(Property = "ShadowMapDimensions")
    Vec2u m_shadowMapDimensions;

    HYP_FIELD(Property = "ShadowMapCascades")
    uint32 m_numShadowMapCascades;

private:
    Pair<Vec3f, Vec3f> CalculateAreaLightRect() const;
};

HYP_CLASS()
class HYP_API DirectionalLight final : public Light
{
    HYP_OBJECT_BODY(DirectionalLight);

public:
    DirectionalLight();
    DirectionalLight(const Vec3f& direction, const Color& color, float intensity);

    virtual ~DirectionalLight() override = default;

    HYP_METHOD()
    const Vec3f& GetDirection() const
    {
        return Light::GetLocalTranslation();
    }

    HYP_METHOD()
    void SetDirection(const Vec3f& direction)
    {
        Light::SetLocalTranslation(direction.Normalized());
    }
};

HYP_CLASS()
class HYP_API PointLight final : public Light
{
    HYP_OBJECT_BODY(PointLight);

public:
    PointLight();
    PointLight(const Vec3f& position, const Color& color, float intensity, float radius);

    virtual ~PointLight() override = default;
};

HYP_CLASS()
class HYP_API SpotLight final : public Light
{
    HYP_OBJECT_BODY(SpotLight);

public:
    SpotLight();
    SpotLight(const Vec3f& position, const Vec3f& direction, const Vec2f& angles, const Color& color, float intensity, float radius);

    virtual ~SpotLight() override = default;
};

HYP_CLASS()
class HYP_API AreaRectLight final : public Light
{
    HYP_OBJECT_BODY(AreaRectLight);

public:
    AreaRectLight();
    AreaRectLight(const Vec3f& position, const Vec3f& normal, const Vec2f& areaSize, const Color& color, float intensity, float radius);

    virtual ~AreaRectLight() override = default;
};

} // namespace Hyperion
