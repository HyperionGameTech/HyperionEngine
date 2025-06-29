/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <scene/Material.hpp>
#include <scene/Texture.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion {

#pragma region RenderMaterial

RenderMaterial::RenderMaterial(Material* material)
    : m_material(material),
      m_buffer_data {}
{
}

RenderMaterial::RenderMaterial(RenderMaterial&& other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase&&>(other)),
      m_material(other.m_material),
      m_textures(std::move(other.m_textures)),
      m_bound_texture_ids(std::move(other.m_bound_texture_ids)),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_material = nullptr;
}

RenderMaterial::~RenderMaterial() = default;

void RenderMaterial::Initialize_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);

    m_render_textures.Reserve(m_textures.Size());

    for (const Pair<MaterialTextureKey, Handle<Texture>>& it : m_textures)
    {
        if (const Handle<Texture>& texture = it.second)
        {
            AssertThrow(texture->IsReady());

            m_render_textures.Set(texture->GetID(), TResourceHandle<RenderTexture>(texture->GetRenderResource()));
        }
    }

    UpdateBufferData();

    HYP_LOG(Material, Debug, "Initializing RenderMaterial: {}", (void*)this);

    if (!g_render_backend->GetRenderConfig().IsBindlessSupported())
    {
        CreateDescriptorSets();
    }
}

void RenderMaterial::Destroy_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);

    m_render_textures.Clear();

    HYP_LOG(Material, Debug, "Destroying RenderMaterial: {}", (void*)this);

    if (!g_render_backend->GetRenderConfig().IsBindlessSupported())
    {
        DestroyDescriptorSets();
    }
}

void RenderMaterial::Update_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);

    static const bool use_bindless_textures = g_render_backend->GetRenderConfig().IsBindlessSupported();

    if (!use_bindless_textures)
    {

        auto set_material_texture = [this](const Handle<Material>& material, uint32 texture_index, const Handle<Texture>& texture)
        {
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
            {
                const DescriptorSetRef& descriptor_set = g_engine->GetMaterialDescriptorSetManager()->GetDescriptorSet(this, frame_index);
                AssertThrow(descriptor_set != nullptr);

                if (texture.IsValid())
                {
                    AssertThrow(texture->GetRenderResource().GetImageView() != nullptr);

                    descriptor_set->SetElement(NAME("Textures"), texture_index, texture->GetRenderResource().GetImageView());
                }
                else
                {
                    descriptor_set->SetElement(NAME("Textures"), texture_index, g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
                }
            }
        };

        Handle<Material> material_locked = m_material->HandleFromThis();

        HYP_LOG(Material, Debug, "Updating Material {} (name: {})", material_locked->GetID(), *material_locked->GetName());

        for (const auto& it : m_textures)
        {
            const uint32 texture_index = uint32(Material::TextureSet::EnumToOrdinal(it.first));

            if (texture_index >= max_bound_textures)
            {
                HYP_LOG(Material, Warning, "Texture index {} is out of bounds of max bound textures ({})", texture_index, max_bound_textures);

                continue;
            }

            const Handle<Texture>& texture = it.second;

            set_material_texture(material_locked, texture_index, texture);
        }
    }
}

GpuBufferHolderBase* RenderMaterial::GetGpuBufferHolder() const
{
    return g_render_global_state->gpu_buffers[GRB_MATERIALS];
}

void RenderMaterial::CreateDescriptorSets()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);

    FixedArray<Handle<Texture>, max_bound_textures> texture_bindings;

    for (const Pair<MaterialTextureKey, Handle<Texture>>& it : m_textures)
    {
        const SizeType texture_index = Material::TextureSet::EnumToOrdinal(it.first);

        if (texture_index >= texture_bindings.Size())
        {
            continue;
        }

        if (const Handle<Texture>& texture = it.second)
        {
            texture_bindings[texture_index] = texture;
        }
    }

    m_descriptor_sets = g_engine->GetMaterialDescriptorSetManager()->AddMaterial(this, std::move(texture_bindings));
}

void RenderMaterial::DestroyDescriptorSets()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);

    g_engine->GetMaterialDescriptorSetManager()->RemoveMaterial(this);
    m_descriptor_sets = {};
}

void RenderMaterial::UpdateBufferData()
{
    HYP_SCOPE;

    AssertThrow(m_buffer_index != ~0u);

    static const bool use_bindless_textures = g_render_backend->GetRenderConfig().IsBindlessSupported();

    m_buffer_data.texture_usage = 0;
    Memory::MemSet(m_buffer_data.texture_index, 0, sizeof(m_buffer_data.texture_index));

    if (m_bound_texture_ids.Any())
    {
        for (SizeType i = 0; i < m_bound_texture_ids.Size(); i++)
        {
            if (m_bound_texture_ids[i] != Id<Texture>::invalid)
            {
                if (use_bindless_textures)
                {
                    m_buffer_data.texture_index[i] = m_bound_texture_ids[i].ToIndex();
                }
                else
                {
                    m_buffer_data.texture_index[i] = i;
                }

                m_buffer_data.texture_usage |= 1 << i;
            }
        }
    }

    *static_cast<MaterialShaderData*>(m_buffer_address) = m_buffer_data;

    GetGpuBufferHolder()->MarkDirty(m_buffer_index);
}

void RenderMaterial::SetTexture(MaterialTextureKey texture_key, const Handle<Texture>& texture)
{
    HYP_SCOPE;

    Execute([this, texture_key, texture]()
        {
            auto it = m_textures.FindAs(texture_key);

            if (it != m_textures.End())
            {
                if (it->second == texture)
                {
                    return;
                }

                m_render_textures.Erase(it->second->GetID());
            }

            m_textures.Set(texture_key, texture);

            if (IsInitialized())
            {
                if (texture.IsValid())
                {
                    AssertThrow(texture->IsReady());

                    m_render_textures.Set(texture->GetID(), TResourceHandle<RenderTexture>(texture->GetRenderResource()));
                }

                UpdateBufferData();
            }
        });
}

void RenderMaterial::SetTextures(FlatMap<MaterialTextureKey, Handle<Texture>>&& textures)
{
    HYP_SCOPE;

    Execute([this, textures = std::move(textures)]()
        {
            m_render_textures.Clear();

            m_textures = std::move(textures);

            if (IsInitialized())
            {
                for (const Pair<MaterialTextureKey, Handle<Texture>>& it : m_textures)
                {
                    if (it.second.IsValid())
                    {
                        AssertThrow(it.second->IsReady());

                        m_render_textures.Set(it.second->GetID(), TResourceHandle<RenderTexture>(it.second->GetRenderResource()));
                    }
                }

                UpdateBufferData();
            }
        });
}

void RenderMaterial::SetBoundTextureIDs(const Array<Id<Texture>>& bound_texture_ids)
{
    HYP_SCOPE;

    Execute([this, bound_texture_ids]()
        {
            m_bound_texture_ids = bound_texture_ids;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

void RenderMaterial::SetBufferData(const MaterialShaderData& buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
        {
            m_buffer_data = buffer_data;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

#pragma endregion RenderMaterial

#pragma region MaterialDescriptorSetManager

MaterialDescriptorSetManager::MaterialDescriptorSetManager()
    : m_pending_addition_flag { false },
      m_descriptor_sets_to_update_flag { 0 }
{
}

MaterialDescriptorSetManager::~MaterialDescriptorSetManager()
{
    for (auto& it : m_material_descriptor_sets)
    {
        SafeRelease(std::move(it.second));
    }

    m_material_descriptor_sets.Clear();

    m_pending_mutex.Lock();

    for (auto& it : m_pending_addition)
    {
        SafeRelease(std::move(it.second));
    }

    m_pending_addition.Clear();
    m_pending_removal.Clear();

    m_pending_mutex.Unlock();
}

void MaterialDescriptorSetManager::CreateInvalidMaterialDescriptorSet()
{
    if (g_render_backend->GetRenderConfig().IsBindlessSupported())
    {
        return;
    }

    const DescriptorSetDeclaration* decl = g_render_global_state->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        m_invalid_material_descriptor_sets[frame_index] = g_render_backend->MakeDescriptorSet(layout);
        m_invalid_material_descriptor_sets[frame_index]->SetDebugName(NAME_FMT("MaterialDescriptorSet_INVALID_{}", frame_index));

        for (uint32 texture_index = 0; texture_index < max_bound_textures; texture_index++)
        {
            m_invalid_material_descriptor_sets[frame_index]->SetElement(NAME("Textures"), texture_index, g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        }

        DeferCreate(m_invalid_material_descriptor_sets[frame_index]);
    }

    m_material_descriptor_sets.Set(nullptr, m_invalid_material_descriptor_sets);
}

const DescriptorSetRef& MaterialDescriptorSetManager::GetDescriptorSet(const RenderMaterial* material, uint32 frame_index) const
{
    if (!material)
    {
        return m_invalid_material_descriptor_sets[frame_index];
    }

    Threads::AssertOnThread(g_render_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_material_descriptor_sets.FindAs(material);

    if (it == m_material_descriptor_sets.End())
    {
        return DescriptorSetRef::unset;
    }

    return it->second[frame_index];
}

FixedArray<DescriptorSetRef, max_frames_in_flight> MaterialDescriptorSetManager::AddMaterial(RenderMaterial* material)
{
    if (!material)
    {
        return {};
    }

    const DescriptorSetDeclaration* decl = g_render_global_state->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(decl != nullptr);

    DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        DescriptorSetRef descriptor_set = g_render_backend->MakeDescriptorSet(layout);

#ifdef HYP_DEBUG_MODE
        descriptor_set->SetDebugName(NAME_FMT("MaterialDescriptorSet_{}_{}", material->GetMaterial()->GetName(), frame_index));
#endif

        for (uint32 texture_index = 0; texture_index < max_bound_textures; texture_index++)
        {
            descriptor_set->SetElement(NAME("Textures"), texture_index, g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        }

        descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    // if on render thread, initialize and add immediately
    if (Threads::IsOnThread(g_render_thread))
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            HYPERION_ASSERT_RESULT(descriptor_sets[frame_index]->Create());
        }

        m_material_descriptor_sets.Insert(material, descriptor_sets);
    }
    else
    {
        Mutex::Guard guard(m_pending_mutex);

        m_pending_addition.PushBack({ material,
            descriptor_sets });

        m_pending_addition_flag.Set(true, MemoryOrder::RELEASE);
    }

    return descriptor_sets;
}

FixedArray<DescriptorSetRef, max_frames_in_flight> MaterialDescriptorSetManager::AddMaterial(RenderMaterial* material, FixedArray<Handle<Texture>, max_bound_textures>&& textures)
{
    if (!material)
    {
        return {};
    }

    const DescriptorSetDeclaration* decl = g_render_global_state->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        DescriptorSetRef descriptor_set = g_render_backend->MakeDescriptorSet(layout);

#ifdef HYP_DEBUG_MODE
        descriptor_set->SetDebugName(NAME_FMT("MaterialDescriptorSet_{}_{}", material->GetMaterial()->GetName(), frame_index));
#endif

        for (uint32 texture_index = 0; texture_index < max_bound_textures; texture_index++)
        {
            if (texture_index < textures.Size())
            {
                const Handle<Texture>& texture = textures[texture_index];

                if (texture.IsValid() && texture->GetRenderResource().GetImageView() != nullptr)
                {
                    descriptor_set->SetElement(NAME("Textures"), texture_index, texture->GetRenderResource().GetImageView());

                    continue;
                }
            }

            descriptor_set->SetElement(NAME("Textures"), texture_index, g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        }

        descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    // if on render thread, initialize and add immediately
    if (Threads::IsOnThread(g_render_thread))
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            HYPERION_ASSERT_RESULT(descriptor_sets[frame_index]->Create());
        }

        m_material_descriptor_sets.Insert(material, descriptor_sets);
    }
    else
    {
        Mutex::Guard guard(m_pending_mutex);

        m_pending_addition.PushBack({ material,
            std::move(descriptor_sets) });

        m_pending_addition_flag.Set(true, MemoryOrder::RELEASE);
    }

    return descriptor_sets;
}

void MaterialDescriptorSetManager::RemoveMaterial(const RenderMaterial* material)
{
    Threads::AssertOnThread(g_render_thread);

    if (!material)
    {
        return;
    }

    { // remove from pending
        Mutex::Guard guard(m_pending_mutex);

        while (true)
        {
            const auto pending_addition_it = m_pending_addition.FindIf([material](const auto& item)
                {
                    return item.first == material;
                });

            if (pending_addition_it == m_pending_addition.End())
            {
                break;
            }

            m_pending_addition.Erase(pending_addition_it);
        }

        const auto pending_removal_it = m_pending_removal.FindAs(material);

        if (pending_removal_it != m_pending_removal.End())
        {
            m_pending_removal.Erase(pending_removal_it);
        }
    }

    const auto material_descriptor_sets_it = m_material_descriptor_sets.FindAs(material);

    if (material_descriptor_sets_it != m_material_descriptor_sets.End())
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            SafeRelease(std::move(material_descriptor_sets_it->second[frame_index]));
        }

        m_material_descriptor_sets.Erase(material_descriptor_sets_it);
    }
}

void MaterialDescriptorSetManager::Initialize()
{
    CreateInvalidMaterialDescriptorSet();
}

void MaterialDescriptorSetManager::UpdatePendingDescriptorSets(FrameBase* frame)
{
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    if (!m_pending_addition_flag.Get(MemoryOrder::ACQUIRE))
    {
        return;
    }

    Mutex::Guard guard(m_pending_mutex);

    for (auto it = m_pending_removal.Begin(); it != m_pending_removal.End();)
    {
        const auto material_descriptor_sets_it = m_material_descriptor_sets.FindAs(*it);

        if (material_descriptor_sets_it != m_material_descriptor_sets.End())
        {
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
            {
                SafeRelease(std::move(material_descriptor_sets_it->second[frame_index]));
            }

            m_material_descriptor_sets.Erase(material_descriptor_sets_it);
        }

        it = m_pending_removal.Erase(it);
    }

    for (auto it = m_pending_addition.Begin(); it != m_pending_addition.End();)
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            AssertThrow(it->second[frame_index].IsValid());

            HYPERION_ASSERT_RESULT(it->second[frame_index]->Create());
        }

        m_material_descriptor_sets.Insert(it->first, std::move(it->second));

        it = m_pending_addition.Erase(it);
    }

    m_pending_addition_flag.Set(false, MemoryOrder::RELEASE);
}

void MaterialDescriptorSetManager::Update(FrameBase* frame)
{
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const uint32 descriptor_sets_to_update_flag = m_descriptor_sets_to_update_flag.Get(MemoryOrder::ACQUIRE);

    if (descriptor_sets_to_update_flag & (1u << frame_index))
    {
        Mutex::Guard guard(m_descriptor_sets_to_update_mutex);

        for (RenderMaterial* material : m_descriptor_sets_to_update[frame_index])
        {
            const auto it = m_material_descriptor_sets.Find(material);

            if (it == m_material_descriptor_sets.End())
            {
                continue;
            }

            AssertThrow(it->second[frame_index].IsValid());
            it->second[frame_index]->Update();
        }

        m_descriptor_sets_to_update[frame_index].Clear();

        m_descriptor_sets_to_update_flag.BitAnd(~(1u << frame_index), MemoryOrder::RELEASE);
    }
}

#pragma endregion MaterialDescriptorSetManager

HYP_DESCRIPTOR_SSBO_COND(Object, MaterialsBuffer, 1, ~0u, false, !g_render_backend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial());
HYP_DESCRIPTOR_SSBO_COND(Object, MaterialsBuffer, 1, sizeof(MaterialShaderData), true, g_render_backend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial());

} // namespace hyperion
