#ifndef HYPERION_V2_LIGHT_H
#define HYPERION_V2_LIGHT_H

#include <core/Base.hpp>
#include <core/lib/Bitset.hpp>
#include <rendering/ShaderDataState.hpp>
#include <rendering/DrawProxy.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <GameCounter.hpp>
#include <Types.hpp>

#include <bitset>

namespace hyperion::v2 {

class Engine;
class Camera;

struct RenderCommand_UpdateLightShaderData;

class Light
    : public BasicObject<STUB_CLASS(Light)>,
      public HasDrawProxy<STUB_CLASS(Light)>
{
    friend struct RenderCommand_UpdateLightShaderData;

public:
    Light(
        LightType type,
        const Vec3f &position,
        const Color &color,
        Float intensity,
        Float radius
    );

    Light(const Light &other) = delete;
    Light &operator=(const Light &other) = delete;

    Light(Light &&other) noexcept;
    Light &operator=(Light &&other) noexcept = delete;
    // Light &operator=(Light &&other) noexcept;

    ~Light();

    LightType GetType() const
        { return m_type; }

    const Vec3f &GetPosition() const
        { return m_position; }

    void SetPosition(const Vec3f &position)
    {
        m_position = position;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    const Color &GetColor() const
        { return m_color; }

    void SetColor(const Color &color)
    {
        m_color = color;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    Float GetIntensity() const
        { return m_intensity; }

    void SetIntensity(Float intensity)
    {
        m_intensity = intensity;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    Float GetRadius() const
        { return m_radius; }

    void SetRadius(Float radius)
    {
        m_radius = radius;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    Float GetFalloff() const
        { return m_falloff; }

    void SetFalloff(Float falloff)
    {
        m_falloff = falloff;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    UInt GetShadowMapIndex() const
        { return m_shadow_map_index; }

    void SetShadowMapIndex(UInt shadow_map_index)
    {
        m_shadow_map_index = shadow_map_index;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    Bool IsVisible(ID<Camera> camera_id) const;
    void SetIsVisible(ID<Camera> camera_id, Bool is_visible);

    BoundingBox GetWorldAABB() const;

    void Init();
    //void EnqueueBind() const;
    void EnqueueUnbind() const;
    void Update();

protected:
    LightType   m_type;
    Vec3f       m_position;
    Color       m_color;
    Float       m_intensity;
    /* Point, spot lights */
    Float       m_radius;
    Float       m_falloff;
    UInt        m_shadow_map_index;

private:
    void EnqueueRenderUpdates();

    mutable ShaderDataState m_shader_data_state;

    Bitset m_visibility_bits;
};

class DirectionalLight : public Light
{
public:
    static constexpr float default_intensity = 100000.0f;
    static constexpr float default_radius = 0.0f;

    DirectionalLight(
        const Vec3f &direction,
        const Color &color,
        Float intensity = default_intensity,
        Float radius = default_radius
    ) : Light(
        LightType::DIRECTIONAL,
        direction,
        color,
        intensity,
        radius
    )
    {
    }

    const Vec3f &GetDirection() const
        { return GetPosition(); }

    void SetDirection(const Vec3f &direction)
        { SetPosition(direction); }
};

class PointLight : public Light
{
public:
    static constexpr float default_intensity = 25.0f;
    static constexpr float default_radius = 15.0f;

    PointLight(
        const Vec3f &position,
        const Color &color,
        Float intensity = default_intensity,
        Float radius = default_radius
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

} // namespace hyperion::v2

#endif
