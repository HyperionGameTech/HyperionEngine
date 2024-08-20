/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Light.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(UnbindLight) : renderer::RenderCommand
{
    ID<Light>   id;

    RENDER_COMMAND(UnbindLight)(ID<Light> id)
        : id(id)
    {
    }

    virtual ~RENDER_COMMAND(UnbindLight)() override = default;

    virtual Result operator()() override
    {
        g_engine->GetRenderState().UnbindLight(id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateLightShaderData) : renderer::RenderCommand
{
    Light           &light;
    LightDrawProxy  proxy;

    RENDER_COMMAND(UpdateLightShaderData)(Light &light, const LightDrawProxy &proxy)
        : light(light),
          proxy(proxy)
    {
    }

    virtual ~RENDER_COMMAND(UpdateLightShaderData)() override = default;

    virtual Result operator()() override
    {
        light.m_proxy = proxy;
        
        if (proxy.visibility_bits == 0) {
            g_engine->GetRenderState().UnbindLight(proxy.id);

            HYP_LOG(Light, LogLevel::DEBUG, "Unbound Light: {}", proxy.id.Value());
        } else {
            g_engine->GetRenderState().BindLight(proxy.id, proxy);

            HYP_LOG(Light, LogLevel::DEBUG, "Bound Light: {}", proxy.id.Value());
        }

        g_engine->GetRenderData()->lights.Set(
            light.GetID().ToIndex(),
            LightShaderData {
                .light_id           = proxy.id.Value(),
                .light_type         = uint32(proxy.type),
                .color_packed       = uint32(proxy.color),
                .radius             = proxy.radius,
                .falloff            = proxy.falloff,
                .shadow_map_index   = proxy.shadow_map_index,
                .area_size          = proxy.area_size,
                .position_intensity = proxy.position_intensity,
                .normal             = proxy.normal,
                .spot_angles        = proxy.spot_angles,
                .material_id        = proxy.material_id.Value()
            }
        );

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

Light::Light() : Light(
    LightType::POINT,
    Vec3f::Zero(),
    Color { 1.0f, 1.0f, 1.0f, 1.0f },
    1.0f,
    1.0f
)
{
}

Light::Light(
    LightType type,
    const Vec3f &position,
    const Color &color,
    float intensity,
    float radius
) : BasicObject(),
    m_type(type),
    m_position(position),
    m_color(color),
    m_intensity(intensity),
    m_radius(radius),
    m_falloff(1.0f),
    m_spot_angles(Vec2f::Zero()),
    m_shadow_map_index(~0u),
    m_mutation_state(DataMutationState::CLEAN)
{
}

Light::Light(
    LightType type,
    const Vec3f &position,
    const Vec3f &normal,
    const Vec2f &area_size,
    const Color &color,
    float intensity,
    float radius
) : BasicObject(),
    m_type(type),
    m_position(position),
    m_normal(normal),
    m_area_size(area_size),
    m_color(color),
    m_intensity(intensity),
    m_radius(radius),
    m_falloff(1.0f),
    m_spot_angles(Vec2f::Zero()),
    m_shadow_map_index(~0u),
    m_mutation_state(DataMutationState::CLEAN)
{
}

Light::Light(Light &&other) noexcept
    : BasicObject(std::move(other)),
      m_type(other.m_type),
      m_position(other.m_position),
      m_normal(other.m_normal),
      m_area_size(other.m_area_size),
      m_color(other.m_color),
      m_intensity(other.m_intensity),
      m_radius(other.m_radius),
      m_falloff(other.m_falloff),
      m_spot_angles(other.m_spot_angles),
      m_shadow_map_index(other.m_shadow_map_index),
      m_visibility_bits(std::move(other.m_visibility_bits)),
      m_mutation_state(DataMutationState::CLEAN),
      m_material(std::move(other.m_material))
{
    other.m_shadow_map_index = ~0u;
}

Light::~Light()
{
    // If material is set for this Light, defer its deletion for a few frames
    if (m_material.IsValid()) {
        g_safe_deleter->SafeRelease(std::move(m_material));
    }

    PUSH_RENDER_COMMAND(UnbindLight, GetID());
}

void Light::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    InitObject(m_material);

    m_proxy = LightDrawProxy {
        .id                 = GetID(),
        .type               = m_type,
        .color              = m_color,
        .radius             = m_radius,
        .falloff            = m_falloff,
        .spot_angles        = m_spot_angles,
        .shadow_map_index   = m_shadow_map_index,
        .area_size          = m_area_size,
        .position_intensity = Vec4f(m_position, m_intensity),
        .normal             = Vec4f(m_normal, 0.0f),
        .visibility_bits    = m_visibility_bits.ToUInt64(),
        .material_id        = m_material.GetID()
    };

    m_mutation_state |= DataMutationState::DIRTY;

    SetReady(true);

    EnqueueRenderUpdates();
}

#if 0
void Light::EnqueueBind() const
{
    Threads::AssertOnThread(~ThreadName::THREAD_RENDER);

    AssertReady();

    PUSH_RENDER_COMMAND(BindLight, m_id);
}
#endif

void Light::EnqueueUnbind() const
{
    Threads::AssertOnThread(~ThreadName::THREAD_RENDER);
    AssertReady();

    PUSH_RENDER_COMMAND(UnbindLight, GetID());
}

void Light::EnqueueRenderUpdates()
{
    AssertReady();

    if (m_material.IsValid() && m_material->GetMutationState().IsDirty()) {
        m_material->EnqueueRenderUpdates();
    }

    if (!m_mutation_state.IsDirty()) {
        return;
    }

    PUSH_RENDER_COMMAND(
        UpdateLightShaderData,
        *this,
        LightDrawProxy {
            .id                 = GetID(),
            .type               = m_type,
            .color              = m_color,
            .radius             = m_radius,
            .falloff            = m_falloff,
            .spot_angles        = m_spot_angles,
            .shadow_map_index   = m_shadow_map_index,
            .area_size          = m_area_size,
            .position_intensity = Vec4f(m_position, m_intensity),
            .normal             = Vec4f(m_normal, 0.0f),
            .visibility_bits    = m_visibility_bits.ToUInt64(),
            .material_id        = m_material.GetID()
        }
    );

    m_mutation_state = DataMutationState::CLEAN;
}

void Light::SetMaterial(Handle<Material> material)
{
    if (material == m_material) {
        return;
    }

    if (m_material.IsValid()) {
        g_safe_deleter->SafeRelease(std::move(m_material));
    }

    m_material = std::move(material);

    if (IsInitCalled()) {
        InitObject(m_material);

        m_mutation_state |= DataMutationState::DIRTY;
    }
}

bool Light::IsVisible(ID<Camera> camera_id) const
{
    return m_visibility_bits.Test(camera_id.ToIndex());
}

void Light::SetIsVisible(ID<Camera> camera_id, bool is_visible)
{
    const bool previous_value = m_visibility_bits.Test(camera_id.ToIndex());

    m_visibility_bits.Set(camera_id.ToIndex(), is_visible);

    if (is_visible != previous_value && IsInitCalled()) {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

Pair<Vec3f, Vec3f> Light::CalculateAreaLightRect() const
{
    Vec3f tangent;
    Vec3f bitangent;
    MathUtil::ComputeOrthonormalBasis(m_normal, tangent, bitangent);

    const float half_width = m_area_size.x * 0.5f;
    const float half_height = m_area_size.y * 0.5f;

    const Vec3f center = m_position;

    const Vec3f p0 = center - tangent * half_width - bitangent * half_height;
    const Vec3f p1 = center + tangent * half_width - bitangent * half_height;
    const Vec3f p2 = center + tangent * half_width + bitangent * half_height;
    const Vec3f p3 = center - tangent * half_width + bitangent * half_height;

    return { p0, p2 };
}

BoundingBox Light::GetAABB() const
{
    if (m_type == LightType::DIRECTIONAL) {
        return BoundingBox::Infinity();
    }

    if (m_type == LightType::AREA_RECT) {
        const Pair<Vec3f, Vec3f> rect = CalculateAreaLightRect();

        BoundingBox aabb;
        aabb.Extend(rect.first);
        aabb.Extend(rect.second);
        aabb.Extend(m_position + m_normal * m_radius);

        return aabb;
    }

    if (m_type == LightType::POINT) {
        return BoundingBox(GetBoundingSphere());
    }

    return BoundingBox::Empty();
}

BoundingSphere Light::GetBoundingSphere() const
{
    if (m_type == LightType::DIRECTIONAL) {
        return BoundingSphere::infinity;
    }

    return BoundingSphere(m_position, m_radius);
}

} // namespace hyperion
