/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderLight.hpp>
#include <rendering/RenderMaterial.hpp>
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
            g_engine->GetRenderState()->UnbindLight(&light->GetRenderResource());
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region LightRenderResource

LightRenderResource::LightRenderResource(Light *light)
    : m_light(light),
      m_buffer_data { }
{
}

LightRenderResource::LightRenderResource(LightRenderResource &&other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase &&>(other)),
      m_light(other.m_light),
      m_visibility_bits(std::move(other.m_visibility_bits)),
      m_material(std::move(other.m_material)),
      m_material_render_resource_handle(std::move(other.m_material_render_resource_handle)),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_light = nullptr;
}

LightRenderResource::~LightRenderResource() = default;

void LightRenderResource::Initialize_Internal()
{
    HYP_SCOPE;
    
    UpdateBufferData();
}

void LightRenderResource::Destroy_Internal()
{
    HYP_SCOPE;
}

void LightRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

void LightRenderResource::EnqueueUnbind()
{
    HYP_SCOPE;

    Execute([this]()
    {
        g_engine->GetRenderState()->UnbindLight(this);
    }, /* force_render_thread */ true);
}

GPUBufferHolderBase *LightRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->lights;
}

void LightRenderResource::UpdateBufferData()
{
    HYP_SCOPE;

    // override material buffer index
    m_buffer_data.material_index = m_material_render_resource_handle
        ? m_material_render_resource_handle->GetBufferIndex()
        : ~0u;

    // GetGPUBufferHolder()->Set(m_buffer_index, m_buffer_data);

    *static_cast<LightShaderData *>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void LightRenderResource::SetMaterial(const Handle<Material> &material)
{
    HYP_SCOPE;

    Execute([this, material]()
    {
        m_material = material;

        if (m_material.IsValid()) {
            m_material_render_resource_handle = TResourceHandle<MaterialRenderResource>(m_material->GetRenderResource());
        } else {
            m_material_render_resource_handle = TResourceHandle<MaterialRenderResource>();
        }

        if (IsInitialized()) {
            SetNeedsUpdate();
        }
    });
}

void LightRenderResource::SetBufferData(const LightShaderData &buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
    {
        m_buffer_data = buffer_data;

        if (IsInitialized()) {
            UpdateBufferData();
        }
    });
}

void LightRenderResource::SetVisibilityBits(const Bitset &visibility_bits)
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
                g_engine->GetRenderState()->BindLight(TResourceHandle<LightRenderResource>(*this));
            } else if (new_count == 0) {
                // May cause Destroy() to be called due to Unclaim() being called.
                g_engine->GetRenderState()->UnbindLight(this);
            }
        }
    }, /* force_render_thread */ true);
}

#pragma endregion LightRenderResource

namespace renderer {

HYP_DESCRIPTOR_SSBO(Scene, CurrentLight, 1, sizeof(LightShaderData), true);
HYP_DESCRIPTOR_SSBO(Scene, LightsBuffer, 1, sizeof(LightShaderData), false);

} // namespace renderer

} // namespace hyperion
