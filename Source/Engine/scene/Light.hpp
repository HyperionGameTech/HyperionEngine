/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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
class Material;
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

    ShadowCacheStaticObjects = 0x10,

    Default = ShadowCaster | ShadowPCF
};

HYP_MAKE_ENUM_FLAGS(LightFlags);

HYP_CLASS()
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
    void SetLightFlags(EnumFlags<LightFlags> flags)
    {
        if (m_lightFlags == flags)
        {
            return;
        }

        m_lightFlags = flags;
        SetNeedsRenderProxyUpdate();
    }

    /*! \brief Get the position for the light. For directional lights, this is the direction the light is pointing.
     *
     *  \return The position or direction. */
    HYP_METHOD(Property = "Position", Editor = true)
    const Vec3f& GetPosition() const
    {
        return m_position;
    }

    /*! \brief Set the position for the light. For directional lights, this is the direction the light is pointing.
     *
     *  \param position The position or direction to set. */
    HYP_METHOD(Property = "Position", Editor = true)
    void SetPosition(const Vec3f& position);

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

    /*! \brief Get the color for the light.
     *
     *  \return The color. */
    HYP_METHOD(Property = "Color", Editor = true)
    const Color& GetColor() const
    {
        return m_color;
    }

    /*! \brief Set the color for the light.
     *
     *  \param color The color to set. */
    HYP_METHOD(Property = "Color", Editor = true)
    void SetColor(const Color& color);

    /*! \brief Get the intensity for the light. This is used to determine how bright the light is.
     *
     *  \return The intensity. */
    HYP_METHOD(Property = "Intensity", Editor = true)
    float GetIntensity() const
    {
        return m_intensity;
    }

    /*! \brief Set the intensity for the light. This is used to determine how bright the light is.
     *
     *  \param intensity The intensity to set. */
    HYP_METHOD(Property = "Intensity", Editor = true)
    void SetIntensity(float intensity);

    /*! \brief Get the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     *  \return The radius. */
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

    /*! \brief Set the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     *  \param radius The radius to set. */
    HYP_METHOD(Property = "Radius", Editor = true)
    void SetRadius(float radius);

    /*! \brief Get the falloff for the light. This is used to determine how the light intensity falls off with distance (point lights only).
     *
     *  \return The falloff. */
    HYP_METHOD(Property = "Falloff", Editor = true)
    float GetFalloff() const
    {
        return m_falloff;
    }

    /*! \brief Set the falloff for the light. This is used to determine how the light intensity falls off with distance (point lights only).
     *
     *  \param falloff The falloff to set. */
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
    const Handle<Material>& GetMaterial() const
    {
        return m_material;
    }

    /*! \brief Sets the material handle associated with the Light. Used for textured area lights.
     *
     *  \param material The material to set for this Light. */
    HYP_METHOD(Property = "Material", Editor = true)
    void SetMaterial(Handle<Material> material);

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

    HYP_METHOD()
    BoundingBox GetAABB() const;

    HYP_METHOD(Property = "ShadowMapFilter", Editor = true, Transient)
    ShadowMapFilter GetShadowMapFilter() const
    {
        return (ShadowMapFilter)((m_lightFlags & LightFlags::ShadowFilterMask)
                ? MathUtil::FastLog2(m_lightFlags & LightFlags::ShadowFilterMask)
                : 0);
    }

    HYP_METHOD(Property = "ShadowMapFilter", Editor = true, Transient)
    void SetShadowMapFilter(ShadowMapFilter shadowMapFilter);

    BoundingSphere GetBoundingSphere() const;

    void UpdateRenderProxy(RenderProxyLight* proxy);

protected:
    void Init() override;
    void Update(float delta) override;

    void OnAttachedToNode(Node* node) override;
    void OnDetachedFromNode(Node* node) override;

    void OnAddedToScene(Scene* scene) override;
    void OnRemovedFromScene(Scene* scene) override;

    void OnTransformUpdated() override;

    HYP_FIELD()
    LightType m_type;

    HYP_FIELD(Property = "LightFlags")
    EnumFlags<LightFlags> m_lightFlags;

    Vec3f m_position;
    Vec3f m_normal;
    Vec2f m_areaSize;
    Color m_color;
    float m_intensity;
    float m_radius;
    float m_falloff;
    Vec2f m_spotAngles;
    Handle<Material> m_material;

    HYP_FIELD(Property = "ShadowMapDimensions")
    Vec2u m_shadowMapDimensions;
    
    HYP_FIELD(Property = "ShadowMapCascades")
    uint32 m_numShadowMapCascades;

private:
    Pair<Vec3f, Vec3f> CalculateAreaLightRect() const;
};

HYP_CLASS()
class HYP_API DirectionalLight : public Light
{
    HYP_OBJECT_BODY(DirectionalLight);

public:
    DirectionalLight()
        : DirectionalLight(Vec3f(0.0f, 1.0f, 0.0f).Normalized(), Color::White(), 1.0f)
    {
    }

    DirectionalLight(const Vec3f& direction, const Color& color, float intensity)
        : Light(LightType::Directional, direction.Normalized(), color, intensity, 0.0f)
    {
        m_lightFlags |= LightFlags::ShadowCacheStaticObjects;
        //m_numShadowMapCascades = 4;
    }

    virtual ~DirectionalLight() override = default;

    HYP_METHOD()
    const Vec3f& GetDirection() const
    {
        return Light::GetPosition();
    }

    HYP_METHOD()
    void SetDirection(const Vec3f& direction)
    {
        Light::SetPosition(direction.Normalized());
    }
};

HYP_CLASS()
class HYP_API PointLight : public Light
{
    HYP_OBJECT_BODY(PointLight);

public:
    PointLight()
        : Light(LightType::Point, Vec3f(0.0f), Color::White(), 5.0f, 10.0f)
    {
    }

    PointLight(const Vec3f& position, const Color& color, float intensity, float radius)
        : Light(LightType::Point, position, color, intensity, radius)
    {
    }

    virtual ~PointLight() override = default;
};

HYP_CLASS()
class HYP_API SpotLight : public Light
{
    HYP_OBJECT_BODY(SpotLight);

public:
    SpotLight()
        : Light(LightType::Spot, Vec3f(0.0f), Vec3f(0.0f, 0.0f, -1.0f), Vec2f(30.0f, 15.0f), Color::White(), 1.0f, 10.0f)
    {
    }

    SpotLight(const Vec3f& position, const Vec3f& direction, const Vec2f& angles, const Color& color, float intensity, float radius)
        : Light(LightType::Spot, position, direction, angles, color, intensity, radius)
    {
    }

    virtual ~SpotLight() override = default;
};

HYP_CLASS()
class HYP_API AreaRectLight : public Light
{
    HYP_OBJECT_BODY(AreaRectLight);

public:
    AreaRectLight()
        : Light(LightType::AreaRect, Vec3f(0.0f), Vec3f(0.0f, 0.0f, -1.0f), Vec2f(1.0f, 1.0f), Color::White(), 1.0f, 10.0f)
    {
    }

    AreaRectLight(const Vec3f& position, const Vec3f& normal, const Vec2f& areaSize, const Color& color, float intensity, float radius)
        : Light(LightType::AreaRect, position, normal, areaSize, color, intensity, radius)
    {
    }

    virtual ~AreaRectLight() override = default;
};

} // namespace Hyperion
