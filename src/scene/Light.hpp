/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/containers/Bitset.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Color.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/MathUtil.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <scene/Entity.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Camera;
class Material;
class View;

enum ShadowMapFilter : uint32;

HYP_ENUM()
enum LightType : uint32
{
    LT_DIRECTIONAL,
    LT_POINT,
    LT_SPOT,
    LT_AREA_RECT,

    LT_MAX
};

HYP_ENUM()
enum LightFlags : uint32
{
    LF_NONE = 0x0,

    LF_SHADOW = 0x1,
    LF_SHADOW_PCF = 0x2,
    LF_SHADOW_CONTACT_HARDENING = 0x4,
    LF_SHADOW_VSM = 0x8,
    LF_SHADOW_FILTER_MASK = (LF_SHADOW_PCF | LF_SHADOW_CONTACT_HARDENING | LF_SHADOW_VSM),

    LF_DEFAULT = LF_SHADOW | LF_SHADOW_PCF
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
        return m_flags;
    }

    HYP_METHOD()
    void SetLightFlags(EnumFlags<LightFlags> flags)
    {
        if (m_flags == flags)
        {
            return;
        }

        m_flags = flags;
        SetNeedsRenderProxyUpdate();
    }

    /*! \brief Get the position for the light. For directional lights, this is the direction the light is pointing.
     *
     *  \return The position or direction. */
    HYP_METHOD(Property = "Position", Serialize = true, Editor = true)
    const Vec3f& GetPosition() const
    {
        return m_position;
    }

    /*! \brief Set the position for the light. For directional lights, this is the direction the light is pointing.
     *
     *  \param position The position or direction to set. */
    HYP_METHOD(Property = "Position", Serialize = true, Editor = true)
    void SetPosition(const Vec3f& position);

    /*! \brief Get the normal for the light. This is used only for area lights.
     *
     *  \return The normal. */
    HYP_METHOD(Property = "Normal", Serialize = true, Editor = true)
    const Vec3f& GetNormal() const
    {
        return m_normal;
    }

    /*! \brief Set the normal for the light. This is used only for area lights.
     *
     *  \param normal The normal to set. */
    HYP_METHOD(Property = "Normal", Serialize = true, Editor = true)
    void SetNormal(const Vec3f& normal);

    /*! \brief Get the area size for the light. This is used only for area lights.
     *
     *  \return The area size. (x = width, y = height) */
    HYP_METHOD(Property = "AreaSize", Serialize = true, Editor = true)
    const Vec2f& GetAreaSize() const
    {
        return m_areaSize;
    }

    /*! \brief Set the area size for the light. This is used only for area lights.
     *
     *  \param areaSize The area size to set. (x = width, y = height) */
    HYP_METHOD(Property = "AreaSize", Serialize = true, Editor = true)
    void SetAreaSize(const Vec2f& areaSize);

    /*! \brief Get the color for the light.
     *
     *  \return The color. */
    HYP_METHOD(Property = "Color", Serialize = true, Editor = true)
    const Color& GetColor() const
    {
        return m_color;
    }

    /*! \brief Set the color for the light.
     *
     *  \param color The color to set. */
    HYP_METHOD(Property = "Color", Serialize = true, Editor = true)
    void SetColor(const Color& color);

    /*! \brief Get the intensity for the light. This is used to determine how bright the light is.
     *
     *  \return The intensity. */
    HYP_METHOD(Property = "Intensity", Serialize = true, Editor = true)
    float GetIntensity() const
    {
        return m_intensity;
    }

    /*! \brief Set the intensity for the light. This is used to determine how bright the light is.
     *
     *  \param intensity The intensity to set. */
    HYP_METHOD(Property = "Intensity", Serialize = true, Editor = true)
    void SetIntensity(float intensity);

    /*! \brief Get the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     *  \return The radius. */
    HYP_METHOD(Property = "Radius", Serialize = true, Editor = true)
    float GetRadius() const
    {
        switch (m_type)
        {
        case LT_DIRECTIONAL:
            return INFINITY;
        case LT_POINT:
            return m_radius;
        default:
            return 0.0f;
        }
    }

    /*! \brief Set the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     *  \param radius The radius to set. */
    HYP_METHOD(Property = "Radius", Serialize = true, Editor = true)
    void SetRadius(float radius);

    /*! \brief Get the falloff for the light. This is used to determine how the light intensity falls off with distance (point lights only).
     *
     *  \return The falloff. */
    HYP_METHOD(Property = "Falloff", Serialize = true, Editor = true)
    float GetFalloff() const
    {
        return m_falloff;
    }

    /*! \brief Set the falloff for the light. This is used to determine how the light intensity falls off with distance (point lights only).
     *
     *  \param falloff The falloff to set. */
    HYP_METHOD(Property = "Falloff", Serialize = true, Editor = true)
    void SetFalloff(float falloff);

    /*! \brief Get the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     *  \return The spotlight angles. */
    HYP_METHOD(Property = "SpotAngles", Serialize = true, Editor = true)
    const Vec2f& GetSpotAngles() const
    {
        return m_spotAngles;
    }

    /*! \brief Set the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     *  \param spotAngles The angles to set for the spotlight. */
    HYP_METHOD(Property = "SpotAngles", Serialize = true, Editor = true)
    void SetSpotAngles(const Vec2f& spotAngles);

    /*! \brief Get the material  for the light. Used for area lights.
     *
     *  \return The material handle associated with the Light. */
    HYP_METHOD(Property = "Material", Serialize = true, Editor = true)
    const Handle<Material>& GetMaterial() const
    {
        return m_material;
    }

    /*! \brief Sets the material handle associated with the Light. Used for textured area lights.
     *
     *  \param material The material to set for this Light. */
    HYP_METHOD(Property = "Material", Serialize = true, Editor = true)
    void SetMaterial(Handle<Material> material);

    HYP_METHOD(Property="ShadowMapDimensions", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const Vec2u& GetShadowMapDimensions() const
    {
        return m_shadowMapDimensions;
    }

    HYP_METHOD(Property="ShadowMapDimensions", Serialize=true, Editor=true)
    void SetShadowMapDimensions(Vec2u shadowMapDimensions);

    HYP_METHOD()
    BoundingBox GetAABB() const;

    HYP_METHOD(Property="ShadowMapFilter", Serialize=false, Editor=true)
    ShadowMapFilter GetShadowMapFilter() const
    {
        return (ShadowMapFilter)((uint32(m_flags) & LF_SHADOW_FILTER_MASK)
            ? MathUtil::FastLog2(uint32(m_flags) & LF_SHADOW_FILTER_MASK)
            : 0);
    }

    HYP_METHOD(Property="ShadowMapFilter", Serialize=false, Editor=true)
    void SetShadowMapFilter(ShadowMapFilter shadowMapFilter);

    BoundingSphere GetBoundingSphere() const;
    
    void UpdateRenderProxy(IRenderProxy* proxy) override final;

protected:
    void Init() override;
    void Update(float delta) override;

    void OnAddedToScene(Scene* scene) override;
    void OnRemovedFromScene(Scene* scene) override;

    void CreateShadowViews();

    HYP_FIELD()
    LightType m_type;

    HYP_FIELD(Property="LightFlags", Serialize)
    EnumFlags<LightFlags> m_flags;

    Vec3f m_position;
    Vec3f m_normal;
    Vec2f m_areaSize;
    Color m_color;
    float m_intensity;
    float m_radius;
    float m_falloff;
    Vec2f m_spotAngles;
    Handle<Material> m_material;
    
    Vec2u m_shadowMapDimensions;
    Array<Handle<View>> m_shadowViews;
    BoundingBox m_shadowAabb;

private:
    Pair<Vec3f, Vec3f> CalculateAreaLightRect() const;
};

HYP_CLASS()
class HYP_API DirectionalLight : public Light
{
    HYP_OBJECT_BODY(DirectionalLight);

public:
    DirectionalLight()
        : Light(LT_DIRECTIONAL, Vec3f(0.0f, 1.0f, 0.0f).Normalized(), Color::White(), 1.0f, 0.0f)
    {
    }

    DirectionalLight(const Vec3f& direction, const Color& color, float intensity)
        : Light(LT_DIRECTIONAL, direction.Normalized(), color, intensity, 0.0f)
    {
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
        : Light(LT_POINT, Vec3f(0.0f), Color::White(), 1.0f, 10.0f)
    {
    }

    PointLight(const Vec3f& position, const Color& color, float intensity, float radius)
        : Light(LT_POINT, position, color, intensity, radius)
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
        : Light(LT_SPOT, Vec3f(0.0f), Vec3f(0.0f, 0.0f, -1.0f), Vec2f(30.0f, 15.0f), Color::White(), 1.0f, 10.0f)
    {
    }

    SpotLight(const Vec3f& position, const Vec3f& direction, const Vec2f& angles, const Color& color, float intensity, float radius)
        : Light(LT_SPOT, position, direction, angles, color, intensity, radius)
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
        : Light(LT_AREA_RECT, Vec3f(0.0f), Vec3f(0.0f, 0.0f, -1.0f), Vec2f(1.0f, 1.0f), Color::White(), 1.0f, 10.0f)
    {
    }

    AreaRectLight(const Vec3f& position, const Vec3f& normal, const Vec2f& areaSize, const Color& color, float intensity, float radius)
        : Light(LT_AREA_RECT, position, normal, areaSize, color, intensity, radius)
    {
    }

    virtual ~AreaRectLight() override = default;
};

} // namespace hyperion

