/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHT_HPP
#define HYPERION_LIGHT_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/containers/Bitset.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Color.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Camera;
class Material;
class RenderLight;

enum class LightType : uint32
{
    DIRECTIONAL,
    POINT,
    SPOT,
    AREA_RECT,

    MAX
};

HYP_CLASS()
class HYP_API Light : public HypObject<Light>
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
        const Vec2f& area_size,
        const Color& color,
        float intensity,
        float radius);

    Light(const Light& other) = delete;
    Light& operator=(const Light& other) = delete;

    Light(Light&& other) noexcept = delete;
    Light& operator=(Light&& other) noexcept = delete;

    ~Light();

    HYP_FORCE_INLINE RenderLight& GetRenderResource() const
    {
        return *m_render_resource;
    }

    /*! \brief Get the current mutation state of the light.
     *
     *  \return The mutation state.
     */
    HYP_FORCE_INLINE DataMutationState GetMutationState() const
    {
        return m_mutation_state;
    }

    /*! \brief Get the type of the light.
     *
     *  \return The type.
     */
    HYP_METHOD(Property = "Type", Serialize = true)
    LightType GetLightType() const
    {
        return m_type;
    }

    /*! \brief Set the type of the light. */
    HYP_METHOD(Property = "Type", Serialize = true)
    void SetLightType(LightType type)
    {
        if (m_type == type)
        {
            return;
        }

        m_type = type;
        m_mutation_state |= DataMutationState::DIRTY;
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
    void SetPosition(const Vec3f& position)
    {
        if (m_position == position)
        {
            return;
        }

        m_position = position;
        m_mutation_state |= DataMutationState::DIRTY;
    }

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
    void SetNormal(const Vec3f& normal)
    {
        if (m_normal == normal)
        {
            return;
        }

        m_normal = normal;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the area size for the light. This is used only for area lights.
     *
     *  \return The area size. (x = width, y = height) */
    HYP_METHOD(Property = "AreaSize", Serialize = true, Editor = true)
    const Vec2f& GetAreaSize() const
    {
        return m_area_size;
    }

    /*! \brief Set the area size for the light. This is used only for area lights.
     *
     *  \param area_size The area size to set. (x = width, y = height) */
    HYP_METHOD(Property = "AreaSize", Serialize = true, Editor = true)
    void SetAreaSize(const Vec2f& area_size)
    {
        if (m_area_size == area_size)
        {
            return;
        }

        m_area_size = area_size;
        m_mutation_state |= DataMutationState::DIRTY;
    }

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
    void SetColor(const Color& color)
    {
        if (m_color == color)
        {
            return;
        }

        m_color = color;
        m_mutation_state |= DataMutationState::DIRTY;
    }

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
    void SetIntensity(float intensity)
    {
        if (m_intensity == intensity)
        {
            return;
        }

        m_intensity = intensity;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the radius for the light. This is used to determine the maximum distance at which this light is visible. (point lights only)
     *
     *  \return The radius. */
    HYP_METHOD(Property = "Radius", Serialize = true, Editor = true)
    float GetRadius() const
    {
        switch (m_type)
        {
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
     *  \param radius The radius to set. */
    HYP_METHOD(Property = "Radius", Serialize = true, Editor = true)
    void SetRadius(float radius)
    {
        if (m_radius == radius)
        {
            return;
        }

        m_radius = radius;
        m_mutation_state |= DataMutationState::DIRTY;
    }

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
    void SetFalloff(float falloff)
    {
        if (m_falloff == falloff)
        {
            return;
        }

        m_falloff = falloff;
        m_mutation_state |= DataMutationState::DIRTY;
    }

    /*! \brief Get the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     *  \return The spotlight angles. */
    HYP_METHOD(Property = "SpotAngles", Serialize = true, Editor = true)
    const Vec2f& GetSpotAngles() const
    {
        return m_spot_angles;
    }

    /*! \brief Set the angles for the spotlight (x = outer, y = inner). This is used to determine the angle of the light cone (spot lights only).
     *
     *  \param spot_angles The angles to set for the spotlight. */
    HYP_METHOD(Property = "SpotAngles", Serialize = true, Editor = true)
    void SetSpotAngles(const Vec2f& spot_angles)
    {
        if (m_spot_angles == spot_angles)
        {
            return;
        }

        m_spot_angles = spot_angles;
        m_mutation_state |= DataMutationState::DIRTY;
    }

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

    HYP_METHOD()
    BoundingBox GetAABB() const;

    BoundingSphere GetBoundingSphere() const;

    void Init();

    void EnqueueRenderUpdates();

protected:
    LightType m_type;
    Vec3f m_position;
    Vec3f m_normal;
    Vec2f m_area_size;
    Color m_color;
    float m_intensity;
    float m_radius;
    float m_falloff;
    Vec2f m_spot_angles;
    Handle<Material> m_material;

private:
    Pair<Vec3f, Vec3f> CalculateAreaLightRect() const;

    mutable DataMutationState m_mutation_state;

    RenderLight* m_render_resource;
};

} // namespace hyperion

#endif
