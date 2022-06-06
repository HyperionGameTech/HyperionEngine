#ifndef HYPERION_V2_LIGHT_H
#define HYPERION_V2_LIGHT_H

#include "base.h"
#include "shader.h"
#include <math/vector3.h>
#include <math/vector4.h>

namespace hyperion::v2 {

class Engine;

enum class LightType {
    DIRECTIONAL,
    POINT,
    SPOT
};

class Light : public EngineComponentBase<STUB_CLASS(Light)> {
public:
    Light(
        LightType type,
        const Vector3 &position,
        const Vector4 &color,
        float intensity
    );

    Light(const Light &other) = delete;
    Light &operator=(const Light &other) = delete;
    ~Light();

    LightType GetType() const { return m_type; }

    const Vector3 &GetPosition() const { return m_position; }

    void SetPosition(const Vector3 &position)
    {
        m_position = position;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    const Vector4 &GetColor() const    { return m_color; }

    void SetColor(const Vector4 &color)
    {
        m_color = color;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    float GetIntensity() const         { return m_intensity; }

    void SetIntensity(float intensity)
    {
        m_intensity = intensity;
        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    void Init(Engine *engine);

protected:
    LightType m_type;
    Vector3   m_position;
    Vector4   m_color;
    float     m_intensity;

private:
    void EnqueueRenderUpdates() const;

    mutable ShaderDataState m_shader_data_state;
};

class DirectionalLight : public Light {
public:
    static constexpr float default_intensity = 100000.0f;

    DirectionalLight(
        const Vector3 &direction,
        const Vector4 &color = Vector4::One(),
        float intensity = default_intensity
    ) : Light(
        LightType::DIRECTIONAL,
        direction,
        color,
        intensity
    )
    {
    }

    const Vector3 &GetDirection() const         { return GetPosition(); }
    void SetDirection(const Vector3 &direction) { SetPosition(direction); }
};

class PointLight : public Light {
public:
    static constexpr float default_intensity = 25.0f;

    PointLight(
        const Vector3 &position,
        const Vector4 &color = Vector4::One(),
        float intensity = default_intensity
    ) : Light(
        LightType::POINT,
        position,
        color,
        intensity
    )
    {
    }
};

} // namespace hyperion::v2

#endif
