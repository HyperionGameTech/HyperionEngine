/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_LIGHT_H
#define HYPERION_V2_LIGHT_H

#include <core/Base.hpp>
#include <core/lib/Bitset.hpp>
#include <rendering/ShaderDataState.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/Material.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <GameCounter.hpp>
#include <Types.hpp>

#include <bitset>

namespace hyperion::v2 {

class Engine;
class Camera;

struct RenderCommand_UpdateLightShaderData;

class HYP_API Light
    : public BasicObject<STUB_CLASS(Light)>,
      public HasDrawProxy<STUB_CLASS(Light)>
{
    friend struct RenderCommand_UpdateLightShaderData;

public:
    Light(
        LightType type,
        const Vec3f &position,
        const Color &color,
        float intensity,
        float radius
    );

    Light(
        LightType type,
        const Vec3f &position,
        const Vec3f &normal,
        const Vec2f &area_size,
        const Color &color,
        float intensity,
        float radius
    );

    Light(const Light &other) = delete;
    Light &operator=(const Light &other) = delete;

    Light(Light &&other) noexcept;
    Light &operator=(Light &&other) noexcept = delete;
    // Light &operator=(Light &&other) noexcept;

    ~Light();

    /*! \brief Get the type of the light.
     *
     * \return The type.
     */
    LightType GetType() const
        { return m_type; }

    /*! \brief Get the position for the light. For directional lights, this is the direction the light is pointing.
     *
     * \return The position or direction.
     */
    Vec3f GetPosition() const
        { return m_position; }

    /*! \brief Set the position for the light. For directional lights, this is the direction the light is pointing.
     *
     * \param position The position or direction to set.
     */
    void SetPosition(Vec3f position)
    {
        if (m_position == position) {
            return;
        }

        m_position = position;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    /*! \brief Get the normal for the light. This is used only for area lights.
     *
     * \return The normal.
     */
    Vec3f GetNormal() const
        { return m_normal; }

    /*! \brief Set the normal for the light. This is used only for area lights.
     *
     * \param normal The normal to set.
     */
    void SetNormal(Vec3f normal)
    {
        if (m_normal == normal) {
            return;
        }

        m_normal = normal;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    /*! \brief Get the area size for the light. This is used only for area lights.
     *
     * \return The area size. (x = width, y = height)
     */
    Vec2f GetAreaSize() const
        { return m_area_size; }

    /*! \brief Set the area size for the light. This is used only for area lights.
     *
     * \param area_size The area size to set. (x = width, y = height)
     */
    void SetAreaSize(Vec2f area_size)
    {
        if (m_area_size == area_size) {
            return;
        }

        m_area_size = area_size;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    /*! \brief Get the color for the light.
     *
     * \return The color.
     */
    Color GetColor() const
        { return m_color; }

    /*! \brief Set the color for the light.
     *
     * \param color The color to set.
     */
    void SetColor(Color color)
    {
        if (m_color == color) {
            return;
        }

        m_color = color;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    /*! \brief Get the intensity for the light. This is used to determine how bright the light is.
     *
     * \return The intensity.
     */
    float GetIntensity() const
        { return m_intensity; }

    /*! \brief Set the intensity for the light. This is used to determine how bright the light is.
     *
     * \param intensity The intensity to set.
     */
    void SetIntensity(float intensity)
    {
        if (m_intensity == intensity) {
            return;
        }

        m_intensity = intensity;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    /*! \brief Get the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     * \return The radius.
     */
    float GetRadius() const
    {
        switch (m_type) {
        case LightType::DIRECTIONAL:
            return INFINITY;
        case LightType::POINT:
            return m_radius;
        default:
            return 0.0f;
        }
    }

    /*! \brief Set the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     * \param radius The radius to set.
     */
    void SetRadius(float radius)
    {
        if (m_type != LightType::POINT) {
            return;
        }

        if (m_radius == radius) {
            return;
        }

        m_radius = radius;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    /*! \brief Get the falloff for the light. This is used to determine how the light intensity falls off with distance (point lights only).
     *
     * \return The falloff.
     */
    float GetFalloff() const
        { return m_falloff; }

    /*! \brief Set the falloff for the light. This is used to determine how the light intensity falls off with distance (point lights only).
     *
     * \param falloff The falloff to set.
     */
    void SetFalloff(float falloff)
    {
        if (m_type != LightType::POINT) {
            return;
        }

        if (m_falloff == falloff) {
            return;
        }

        m_falloff = falloff;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    /*! \brief Get the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     * \return The spotlight angles.
     */
    Vec2f GetSpotAngles() const
        { return m_spot_angles; }

    /*! \brief Set the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     * \param spot_angles The angles to set for the spotlight.
     */
    void SetSpotAngles(Vec2f spot_angles)
    {
        if (m_type != LightType::SPOT) {
            return;
        }

        if (m_spot_angles == spot_angles) {
            return;
        }

        m_spot_angles = spot_angles;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    /*! \brief Get the shadow map index for the light. This is used when sampling shadow maps for the particular light.
     *
     * \return The shadow map index.
     */
    uint GetShadowMapIndex() const
        { return m_shadow_map_index; }

    /*! \brief Set the shadow map index for the light. This is used when sampling shadow maps for the particular light.
     *
     * \param shadow_map_index The shadow map index to set.
     */
    void SetShadowMapIndex(uint shadow_map_index)
    {
        if (shadow_map_index == m_shadow_map_index) {
            return;
        }

        m_shadow_map_index = shadow_map_index;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    /*! \brief Get the material  for the light. Used for area lights.
     *
     * \return The material handle associated with the Light.
     */
    const Handle<Material> &GetMaterial() const
        { return m_material; }

    /*! \brief Sets the material handle associated with the Light. Used for textured area lights.
     *
     * \param material The material to set for this Light.
     */
    void SetMaterial(Handle<Material> material)
    {
        if (material == m_material) {
            return;
        }

        m_material = std::move(material);
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    /*! \brief Check if the light is set as visible to the camera.
     *
     * \param camera_id The camera to check visibility for.
     * \return True if the light is visible, false otherwise.
     */
    bool IsVisible(ID<Camera> camera_id) const;

    /*! \brief Set the visibility of the light to the camera.
     *
     * \param camera_id The camera to set visibility for.
     * \param is_visible True if the light is visible, false otherwise.
     */
    void SetIsVisible(ID<Camera> camera_id, bool is_visible);

    BoundingBox GetAABB() const;
    BoundingSphere GetBoundingSphere() const;

    void Init();
    //void EnqueueBind() const;
    void EnqueueUnbind() const;
    void Update();

protected:
    LightType           m_type;
    Vec3f               m_position;
    Vec3f               m_normal;
    Vec2f               m_area_size;
    Color               m_color;
    float               m_intensity;
    float               m_radius;
    float               m_falloff;
    Vec2f               m_spot_angles;
    uint                m_shadow_map_index;
    Handle<Material>    m_material;

private:
    Pair<Vec3f, Vec3f> CalculateAreaLightRect() const;

    void EnqueueRenderUpdates();

    mutable ShaderDataState m_shader_data_state;

    Bitset                  m_visibility_bits;
};

class HYP_API DirectionalLight : public Light
{
public:
    static constexpr float default_intensity = 10.0f;

    DirectionalLight(
        Vec3f direction,
        Color color,
        float intensity = default_intensity
    ) : Light(
            LightType::DIRECTIONAL,
            direction,
            color,
            intensity,
            0.0f
        )
    {
    }

    Vec3f GetDirection() const
        { return GetPosition(); }

    void SetDirection(Vec3f direction)
        { SetPosition(direction); }
};

class HYP_API PointLight : public Light
{
public:
    static constexpr float default_intensity = 5.0f;
    static constexpr float default_radius = 15.0f;

    PointLight(
        Vec3f position,
        Color color,
        float intensity = default_intensity,
        float radius = default_radius
    ) : Light(
            LightType::POINT,
            position,
            color,
            intensity,
            radius
        )
    {
    }
};

class HYP_API SpotLight : public Light
{
public:
    static constexpr float default_intensity = 5.0f;
    static constexpr float default_radius = 15.0f;
    static constexpr float default_outer_angle = 45.0f;
    static constexpr float default_inner_angle = 30.0f;

    SpotLight(
        Vec3f position,
        Vec3f direction,
        Color color,
        float intensity = default_intensity,
        float radius = default_radius,
        Vec2f angles = { default_outer_angle, default_inner_angle }
    ) : Light(
            LightType::SPOT,
            position,
            direction,
            Vec2f(0.0f, 0.0f),
            color,
            intensity,
            radius
        )
    {
        SetSpotAngles(angles);
    }

    Vec3f GetDirection() const
        { return GetNormal(); }

    void SetDirection(Vec3f direction)
        { SetNormal(direction); }
};

class HYP_API RectangleLight : public Light
{
public:
    RectangleLight(
        Vec3f position,
        Vec3f normal,
        Vec2f area_size,
        Color color,
        float intensity = 1.0f,
        float distance = 1.0f
    ) : Light(
            LightType::AREA_RECT,
            position,
            normal,
            area_size,
            color,
            intensity,
            distance
        )
    {
    }
};

} // namespace hyperion::v2

#endif
