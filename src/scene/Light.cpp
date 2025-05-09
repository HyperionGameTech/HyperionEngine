/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Light.hpp>
#include <scene/Material.hpp>

#include <rendering/RenderLight.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Light

Light::Light() : Light(
    LightType::DIRECTIONAL,
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
) : HypObject(),
    m_type(type),
    m_position(position),
    m_color(color),
    m_intensity(intensity),
    m_radius(radius),
    m_falloff(1.0f),
    m_spot_angles(Vec2f::Zero()),
    m_mutation_state(DataMutationState::CLEAN),
    m_render_resource(nullptr)
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
) : HypObject(),
    m_type(type),
    m_position(position),
    m_normal(normal),
    m_area_size(area_size),
    m_color(color),
    m_intensity(intensity),
    m_radius(radius),
    m_falloff(1.0f),
    m_spot_angles(Vec2f::Zero()),
    m_mutation_state(DataMutationState::CLEAN),
    m_render_resource(nullptr)
{
}

Light::~Light()
{
    if (m_render_resource != nullptr) {
        FreeResource(m_render_resource);
    }
}

void Light::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    m_render_resource = AllocateResource<LightRenderResource>(this);

    if (m_material.IsValid()) {
        InitObject(m_material);

        m_render_resource->SetMaterial(m_material);
    }

    m_mutation_state |= DataMutationState::DIRTY;

    SetReady(true);

    EnqueueRenderUpdates();
}

void Light::EnqueueRenderUpdates()
{
    AssertReady();

    if (m_material.IsValid() && m_material->GetMutationState().IsDirty()) {
        m_mutation_state |= DataMutationState::DIRTY;

        m_material->EnqueueRenderUpdates();
    }

    if (!m_mutation_state.IsDirty()) {
        return;
    }

    LightShaderData buffer_data {
        .light_id           = GetID().Value(),
        .light_type         = uint32(m_type),
        .color_packed       = uint32(m_color),
        .radius             = m_radius,
        .falloff            = m_falloff,
        .area_size          = m_area_size,
        .position_intensity = Vec4f(m_position, m_intensity),
        .normal             = Vec4f(m_normal, 0.0f),
        .spot_angles        = m_spot_angles,
        .material_index     = ~0u // set later
    };

    m_render_resource->SetBufferData(buffer_data);
    m_render_resource->SetVisibilityBits(m_visibility_bits);

    m_mutation_state = DataMutationState::CLEAN;
}

void Light::SetMaterial(Handle<Material> material)
{
    if (material == m_material) {
        return;
    }

    m_material = std::move(material);

    if (IsInitCalled()) {
        InitObject(m_material);

        m_render_resource->SetMaterial(m_material);

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

        return BoundingBox::Empty()
            .Union(rect.first)
            .Union(rect.second)
            .Union(m_position + m_normal * m_radius);
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

#pragma endregion Light

} // namespace hyperion
