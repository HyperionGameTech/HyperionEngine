/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Light.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderState.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(UnbindLight) : renderer::RenderCommand
{
    WeakHandle<Light>   light_weak;

    RENDER_COMMAND(UnbindLight)(const WeakHandle<Light> &light_weak)
        : light_weak(light_weak)
    {
    }

    virtual ~RENDER_COMMAND(UnbindLight)() override = default;

    virtual RendererResult operator()() override
    {
        if (Handle<Light> light = light_weak.Lock()) {
            g_engine->GetRenderState()->UnbindLight(light.Get());
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region LightRenderResources

LightRenderResources::LightRenderResources(Light *light)
    : m_light(light),
      m_buffer_data { }
{
}

LightRenderResources::LightRenderResources(LightRenderResources &&other) noexcept
    : RenderResourcesBase(static_cast<RenderResourcesBase &&>(other)),
      m_light(other.m_light),
      m_visibility_bits(std::move(other.m_visibility_bits)),
      m_material(std::move(other.m_material)),
      m_material_render_resources_handle(std::move(other.m_material_render_resources_handle)),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_light = nullptr;
}

LightRenderResources::~LightRenderResources() = default;

void LightRenderResources::Initialize()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void LightRenderResources::Destroy()
{
    HYP_SCOPE;
}

void LightRenderResources::Update()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *LightRenderResources::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->lights.Get();
}

void LightRenderResources::UpdateBufferData()
{
    HYP_SCOPE;

    // override material buffer index
    m_buffer_data.material_index = m_material_render_resources_handle
        ? m_material_render_resources_handle->GetBufferIndex()
        : ~0u;

    // GetGPUBufferHolder()->Set(m_buffer_index, m_buffer_data);

    *static_cast<LightShaderData *>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void LightRenderResources::SetMaterial(const Handle<Material> &material)
{
    HYP_SCOPE;

    Execute([this, material]()
    {
        m_material = material;

        if (m_material.IsValid()) {
            m_material_render_resources_handle = TResourceHandle<MaterialRenderResources>(m_material->GetRenderResources());
        } else {
            m_material_render_resources_handle = TResourceHandle<MaterialRenderResources>();
        }

        if (m_is_initialized) {
            SetNeedsUpdate();
        }
    });
}

void LightRenderResources::SetBufferData(const LightShaderData &buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
    {
        m_buffer_data = buffer_data;

        if (m_is_initialized) {
            UpdateBufferData();
        }
    });
}

void LightRenderResources::SetVisibilityBits(const Bitset &visibility_bits)
{
    HYP_SCOPE;

    Execute([this, visibility_bits]()
    {
        const SizeType previous_count = m_visibility_bits.Count();
        const SizeType new_count = visibility_bits.Count();

        m_visibility_bits = visibility_bits;

        if (previous_count != new_count) {
            if (previous_count == 0) {
                // May cause Initialize() to be called due to Claim() being called when binding a light.
                g_engine->GetRenderState()->BindLight(m_light);
            } else if (new_count == 0) {
                    // May cause Destroy() to be called due to Unclaim() being called.
                g_engine->GetRenderState()->UnbindLight(m_light);
            }
        }
    }, /* force_render_thread */ true);
}

#pragma endregion LightRenderResources

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
    m_shadow_map_index(~0u),
    m_mutation_state(DataMutationState::CLEAN),
    m_render_resources(nullptr)
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
    m_shadow_map_index(~0u),
    m_mutation_state(DataMutationState::CLEAN),
    m_render_resources(nullptr)
{
}

Light::~Light()
{
    if (m_render_resources != nullptr) {
        FreeResource(m_render_resources);
    }
    
    // If material is set for this Light, defer its deletion for a few frames
    if (m_material.IsValid()) {
        g_safe_deleter->SafeRelease(std::move(m_material));
    }
}

void Light::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    m_render_resources = AllocateResource<LightRenderResources>(this);

    if (m_material.IsValid()) {
        InitObject(m_material);

        m_render_resources->SetMaterial(m_material);
    }

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
    AssertReady();

    PUSH_RENDER_COMMAND(UnbindLight, WeakHandleFromThis());
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
        .shadow_map_index   = m_shadow_map_index,
        .area_size          = m_area_size,
        .position_intensity = Vec4f(m_position, m_intensity),
        .normal             = Vec4f(m_normal, 0.0f),
        .spot_angles        = m_spot_angles,
        .material_index     = ~0u // set later
    };

    m_render_resources->SetBufferData(buffer_data);
    m_render_resources->SetVisibilityBits(m_visibility_bits);

    m_mutation_state = DataMutationState::CLEAN;
}

void Light::SetMaterial(Handle<Material> material)
{
    if (material == m_material) {
        return;
    }

    if (m_material.IsValid() && IsInitCalled()) {
        g_safe_deleter->SafeRelease(std::move(m_material));
    }

    m_material = std::move(material);

    if (IsInitCalled()) {
        InitObject(m_material);

        m_render_resources->SetMaterial(m_material);

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

namespace renderer {

HYP_DESCRIPTOR_SSBO(Scene, LightsBuffer, 1, sizeof(LightShaderData), true);

} // namespace renderer

} // namespace hyperion
