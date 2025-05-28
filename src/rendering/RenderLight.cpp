/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderLight.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <scene/Material.hpp>
#include <scene/Light.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderLight

RenderLight::RenderLight(Light* light)
    : m_light(light),
      m_buffer_data {}
{
}

RenderLight::RenderLight(RenderLight&& other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase&&>(other)),
      m_light(other.m_light),
      m_material(std::move(other.m_material)),
      m_render_material(std::move(other.m_render_material)),
      m_shadow_render_map(std::move(other.m_shadow_render_map)),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_light = nullptr;
}

RenderLight::~RenderLight() = default;

void RenderLight::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void RenderLight::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderLight::Update_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

GPUBufferHolderBase* RenderLight::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->lights;
}

void RenderLight::UpdateBufferData()
{
    HYP_SCOPE;

    LightShaderData* buffer_data = static_cast<LightShaderData*>(m_buffer_address);

    *buffer_data = m_buffer_data;

    // override material buffer index
    buffer_data->material_index = m_render_material
        ? m_render_material->GetBufferIndex()
        : ~0u;

    if (m_shadow_render_map)
    {
        buffer_data->shadow_map_index = m_shadow_render_map->GetBufferIndex();
    }
    else
    {
        buffer_data->shadow_map_index = ~0u;
    }

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void RenderLight::SetMaterial(const Handle<Material>& material)
{
    HYP_SCOPE;

    Execute([this, material]()
        {
            m_material = material;

            if (m_material.IsValid())
            {
                m_render_material = TResourceHandle<RenderMaterial>(m_material->GetRenderResource());
            }
            else
            {
                m_render_material = TResourceHandle<RenderMaterial>();
            }

            if (IsInitialized())
            {
                SetNeedsUpdate();
            }
        });
}

void RenderLight::SetBufferData(const LightShaderData& buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
        {
            m_buffer_data = buffer_data;

            if (m_shadow_render_map)
            {
                m_buffer_data.shadow_map_index = m_shadow_render_map->GetBufferIndex();
            }
            else
            {
                m_buffer_data.shadow_map_index = ~0u;
            }

            if (IsInitialized())
            {
                SetNeedsUpdate();
            }
        });
}

void RenderLight::SetShadowMapResourceHandle(TResourceHandle<RenderShadowMap>&& shadow_render_map)
{
    HYP_SCOPE;

    Execute([this, shadow_render_map = std::move(shadow_render_map)]()
        {
            m_shadow_render_map = std::move(shadow_render_map);

            if (m_shadow_render_map)
            {
                m_buffer_data.shadow_map_index = m_shadow_render_map->GetBufferIndex();
            }
            else
            {
                m_buffer_data.shadow_map_index = ~0u;
            }

            if (IsInitialized())
            {
                SetNeedsUpdate();
            }
        });
}

#pragma endregion RenderLight

namespace renderer {

HYP_DESCRIPTOR_SSBO(Global, CurrentLight, 1, sizeof(LightShaderData), true);
HYP_DESCRIPTOR_SSBO(Global, LightsBuffer, 1, sizeof(LightShaderData), false);

} // namespace renderer

} // namespace hyperion
