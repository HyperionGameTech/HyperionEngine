/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderMaterial.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <scene/Material.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion {

#pragma region MaterialRenderResource

MaterialRenderResource::MaterialRenderResource(Material *material)
    : m_material(material),
      m_buffer_data { }
{
}

MaterialRenderResource::MaterialRenderResource(MaterialRenderResource &&other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase &&>(other)),
      m_material(other.m_material),
      m_textures(std::move(other.m_textures)),
      m_bound_texture_ids(std::move(other.m_bound_texture_ids)),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_material = nullptr;
}

MaterialRenderResource::~MaterialRenderResource() = default;

void MaterialRenderResource::Initialize_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);
    
    UpdateBufferData();

    if (!g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
        CreateDescriptorSets();
    }
}

void MaterialRenderResource::Destroy_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);
    
    if (!g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
        DestroyDescriptorSets();
    }
}

void MaterialRenderResource::Update_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);
    
    const bool use_bindless_textures = g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures();

    auto SetMaterialTexture = [](const Handle<Material> &material, uint32 texture_index, const Handle<Texture> &texture)
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSetRef &descriptor_set = g_engine->GetMaterialDescriptorSetManager()->GetDescriptorSet(material.GetID(), frame_index);
            AssertThrow(descriptor_set != nullptr);

            if (texture.IsValid()) {
                AssertThrow(texture->GetImageView() != nullptr);

                descriptor_set->SetElement(NAME("Textures"), texture_index, texture->GetImageView());
            } else {
                descriptor_set->SetElement(NAME("Textures"), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
            }
        }

        g_engine->GetMaterialDescriptorSetManager()->SetNeedsDescriptorSetUpdate(material);
    };

    if (!use_bindless_textures) {
        Handle<Material> material_locked = m_material->HandleFromThis();

        for (const auto &it : m_textures) {
            const uint32 texture_index = uint32(Material::TextureSet::EnumToOrdinal(it.first));

            if (texture_index >= max_bound_textures) {
                HYP_LOG(Material, Warning, "Texture index {} is out of bounds of max bound textures ({})", texture_index, max_bound_textures);

                continue;
            }

            const Handle<Texture> &texture = it.second;

            SetMaterialTexture(material_locked, texture_index, texture);
        }
    }
}

GPUBufferHolderBase *MaterialRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->materials;
}

void MaterialRenderResource::CreateDescriptorSets()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);

    FixedArray<Handle<Texture>, max_bound_textures> texture_bindings;

    for (const Pair<MaterialTextureKey, Handle<Texture>> &it : m_textures) {
        const SizeType texture_index = Material::TextureSet::EnumToOrdinal(it.first);

        if (texture_index >= texture_bindings.Size()) {
            continue;
        }

        if (const Handle<Texture> &texture = it.second) {
            texture_bindings[texture_index] = texture;
        }
    }

    m_descriptor_sets = g_engine->GetMaterialDescriptorSetManager()->AddMaterial(m_material->HandleFromThis(), std::move(texture_bindings));
}

void MaterialRenderResource::DestroyDescriptorSets()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);
    
    g_engine->GetMaterialDescriptorSetManager()->RemoveMaterial(m_material->GetID());
    m_descriptor_sets = { };
}

void MaterialRenderResource::UpdateBufferData()
{
    HYP_SCOPE;

    AssertThrow(m_buffer_index != ~0u);

    const bool use_bindless_textures = g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures();

    m_buffer_data.texture_usage = 0;
    Memory::MemSet(m_buffer_data.texture_index, 0, sizeof(m_buffer_data.texture_index));

    if (m_bound_texture_ids.Any()) {
        for (SizeType i = 0; i < m_bound_texture_ids.Size(); i++) {
            if (m_bound_texture_ids[i] != ID<Texture>::invalid) {
                if (use_bindless_textures) {
                    m_buffer_data.texture_index[i] = m_bound_texture_ids[i].ToIndex();
                } else {
                    m_buffer_data.texture_index[i] = i;
                }

                m_buffer_data.texture_usage |= 1 << i;
            }
        }
    }

    *static_cast<MaterialShaderData *>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void MaterialRenderResource::SetTexture(MaterialTextureKey texture_key, const Handle<Texture> &texture)
{
    HYP_SCOPE;

    Execute([this, texture_key, texture]()
    {
        m_textures.Set(texture_key, texture);

        if (IsInitialized()) {
            UpdateBufferData();
        }
    });
}

void MaterialRenderResource::SetTextures(FlatMap<MaterialTextureKey, Handle<Texture>> &&textures)
{
    HYP_SCOPE;

    Execute([this, textures = std::move(textures)]()
    {
        m_textures = std::move(textures);

        if (IsInitialized()) {
            UpdateBufferData();
        }
    });
}

void MaterialRenderResource::SetBoundTextureIDs(const Array<ID<Texture>> &bound_texture_ids)
{
    HYP_SCOPE;

    Execute([this, bound_texture_ids]()
    {
        m_bound_texture_ids = bound_texture_ids;

        if (IsInitialized()) {
            UpdateBufferData();
        }
    });
}

void MaterialRenderResource::SetBufferData(const MaterialShaderData &buffer_data)
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

#pragma endregion MaterialRenderResource

#pragma region MaterialDescriptorSetManager

MaterialDescriptorSetManager::MaterialDescriptorSetManager()
    : m_pending_addition_flag { false },
      m_descriptor_sets_to_update_flag { 0 }
{
}

MaterialDescriptorSetManager::~MaterialDescriptorSetManager()
{
    for (auto &it : m_material_descriptor_sets) {
        SafeRelease(std::move(it.second));
    }

    m_material_descriptor_sets.Clear();

    m_pending_mutex.Lock();

    for (auto &it : m_pending_addition) {
        SafeRelease(std::move(it.second));
    }

    m_pending_addition.Clear();
    m_pending_removal.Clear();

    m_pending_mutex.Unlock();
}

void MaterialDescriptorSetManager::CreateInvalidMaterialDescriptorSet()
{
    if (g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
        return;
    }

    const renderer::DescriptorSetDeclaration *declaration = g_engine->GetGlobalDescriptorTable()->GetDeclaration().FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(declaration != nullptr);

    const renderer::DescriptorSetLayout layout(*declaration);

    const DescriptorSetRef invalid_descriptor_set = layout.CreateDescriptorSet();
    
    for (uint32 texture_index = 0; texture_index < max_bound_textures; texture_index++) {
        invalid_descriptor_set->SetElement(NAME("Textures"), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
    }

    DeferCreate(invalid_descriptor_set, g_engine->GetGPUDevice());

    m_material_descriptor_sets.Set(WeakHandle<Material> {}, { invalid_descriptor_set, invalid_descriptor_set });
}

const DescriptorSetRef &MaterialDescriptorSetManager::GetDescriptorSet(ID<Material> id, uint32 frame_index) const
{
    Threads::AssertOnThread(g_render_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_material_descriptor_sets.FindAs(id);

    if (it == m_material_descriptor_sets.End()) {
        return DescriptorSetRef::unset;
    }

    return it->second[frame_index];
}

FixedArray<DescriptorSetRef, max_frames_in_flight> MaterialDescriptorSetManager::AddMaterial(const Handle<Material> &material)
{
    if (!material.IsValid()) {
        return { };
    }

    const renderer::DescriptorSetDeclaration *declaration = g_engine->GetGlobalDescriptorTable()->GetDeclaration().FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(declaration != nullptr);

    renderer::DescriptorSetLayout layout(*declaration);

    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        descriptor_sets[frame_index] = MakeRenderObject<DescriptorSet>(layout);

        for (uint32 texture_index = 0; texture_index < max_bound_textures; texture_index++) {
            descriptor_sets[frame_index]->SetElement(NAME("Textures"), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }
    }

    // if on render thread, initialize and add immediately
    if (Threads::IsOnThread(g_render_thread)) {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_ASSERT_RESULT(descriptor_sets[frame_index]->Create(g_engine->GetGPUDevice()));
        }

        m_material_descriptor_sets.Insert(material, descriptor_sets);
    } else {
        Mutex::Guard guard(m_pending_mutex);

        m_pending_addition.PushBack({
            material,
            descriptor_sets
        });

        m_pending_addition_flag.Set(true, MemoryOrder::RELEASE);
    }

    return descriptor_sets;
}

FixedArray<DescriptorSetRef, max_frames_in_flight> MaterialDescriptorSetManager::AddMaterial(const Handle<Material> &material, FixedArray<Handle<Texture>, max_bound_textures> &&textures)
{
    if (!material.IsValid()) {
        return { };
    }

    const renderer::DescriptorSetDeclaration *declaration = g_engine->GetGlobalDescriptorTable()->GetDeclaration().FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(declaration != nullptr);

    const renderer::DescriptorSetLayout layout(*declaration);

    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        descriptor_sets[frame_index] = layout.CreateDescriptorSet();

        for (uint32 texture_index = 0; texture_index < max_bound_textures; texture_index++) {
            if (texture_index < textures.Size()) {
                const Handle<Texture> &texture = textures[texture_index];

                if (texture.IsValid() && texture->GetImageView() != nullptr) {
                    descriptor_sets[frame_index]->SetElement(NAME("Textures"), texture_index, texture->GetImageView());

                    continue;
                }
            }

            descriptor_sets[frame_index]->SetElement(NAME("Textures"), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }
    }

    // if on render thread, initialize and add immediately
    if (Threads::IsOnThread(g_render_thread)) {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_ASSERT_RESULT(descriptor_sets[frame_index]->Create(g_engine->GetGPUDevice()));
        }

        m_material_descriptor_sets.Insert(material, descriptor_sets);
    } else {
        Mutex::Guard guard(m_pending_mutex);

        m_pending_addition.PushBack({
            material,
            std::move(descriptor_sets)
        });

        m_pending_addition_flag.Set(true, MemoryOrder::RELEASE);
    }

    return descriptor_sets;
}

void MaterialDescriptorSetManager::EnqueueRemoveMaterial(const WeakHandle<Material> &material)
{
    if (!material.IsValid()) {
        return;
    }

    HYP_LOG(Material, Debug, "EnqueueRemove material with ID {} from thread {}", material.GetID().Value(), Threads::CurrentThreadID().GetName());

    Mutex::Guard guard(m_pending_mutex);
    
    while (true) {
        const auto pending_addition_it = m_pending_addition.FindIf([&material](const auto &item)
        {
            return item.first == material;
        });

        if (pending_addition_it == m_pending_addition.End()) {
            break;
        }

        m_pending_addition.Erase(pending_addition_it);
    }

    if (!m_pending_removal.Contains(material)) {
        m_pending_removal.PushBack(material);
    }

    m_pending_addition_flag.Set(true, MemoryOrder::RELEASE);
}

void MaterialDescriptorSetManager::RemoveMaterial(ID<Material> id)
{
    Threads::AssertOnThread(g_render_thread);

    if (!id.IsValid()) {
        return;
    }

    { // remove from pending
        Mutex::Guard guard(m_pending_mutex);
    
        while (true) {
            const auto pending_addition_it = m_pending_addition.FindIf([id](const auto &item)
            {
                return item.first.GetID() == id;
            });

            if (pending_addition_it == m_pending_addition.End()) {
                break;
            }

            m_pending_addition.Erase(pending_addition_it);
        }

        const auto pending_removal_it = m_pending_removal.FindAs(id);

        if (pending_removal_it != m_pending_removal.End()) {
            m_pending_removal.Erase(pending_removal_it);
        }
    }

    const auto material_descriptor_sets_it = m_material_descriptor_sets.FindAs(id);

    if (material_descriptor_sets_it != m_material_descriptor_sets.End()) {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            SafeRelease(std::move(material_descriptor_sets_it->second[frame_index]));
        }

        m_material_descriptor_sets.Erase(material_descriptor_sets_it);
    }
}

void MaterialDescriptorSetManager::SetNeedsDescriptorSetUpdate(const Handle<Material> &material)
{
    if (!material.IsValid()) {
        return;
    }

    Mutex::Guard guard(m_descriptor_sets_to_update_mutex);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const auto it = m_descriptor_sets_to_update[frame_index].FindAs(material);

        if (it != m_descriptor_sets_to_update[frame_index].End()) {
            continue;
        }

        m_descriptor_sets_to_update[frame_index].PushBack(material);
    }

    m_descriptor_sets_to_update_flag.Set(0x3, MemoryOrder::RELEASE);
}

void MaterialDescriptorSetManager::Initialize()
{
    CreateInvalidMaterialDescriptorSet();
}

void MaterialDescriptorSetManager::UpdatePendingDescriptorSets(Frame *frame)
{
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    if (!m_pending_addition_flag.Get(MemoryOrder::ACQUIRE)) {
        return;
    }

    Mutex::Guard guard(m_pending_mutex);

    for (auto it = m_pending_removal.Begin(); it != m_pending_removal.End();) {
        const auto material_descriptor_sets_it = m_material_descriptor_sets.FindAs(*it);

        if (material_descriptor_sets_it != m_material_descriptor_sets.End()) {
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                SafeRelease(std::move(material_descriptor_sets_it->second[frame_index]));
            }

            m_material_descriptor_sets.Erase(material_descriptor_sets_it);
        }

        it = m_pending_removal.Erase(it);
    }

    for (auto it = m_pending_addition.Begin(); it != m_pending_addition.End();) {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            AssertThrow(it->second[frame_index].IsValid());

            HYPERION_ASSERT_RESULT(it->second[frame_index]->Create(g_engine->GetGPUDevice()));
        }

        m_material_descriptor_sets.Insert(it->first, std::move(it->second));

        it = m_pending_addition.Erase(it);
    }

    m_pending_addition_flag.Set(false, MemoryOrder::RELEASE);
}

void MaterialDescriptorSetManager::Update(Frame *frame)
{
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const uint32 descriptor_sets_to_update_flag = m_descriptor_sets_to_update_flag.Get(MemoryOrder::ACQUIRE);

    if (descriptor_sets_to_update_flag & (1u << frame_index)) {
        Mutex::Guard guard(m_descriptor_sets_to_update_mutex);

        for (const WeakHandle<Material> &material : m_descriptor_sets_to_update[frame_index]) {
            const auto it = m_material_descriptor_sets.Find(material);

            if (it == m_material_descriptor_sets.End()) {
                continue;
            }

            AssertThrow(it->second[frame_index].IsValid());
            it->second[frame_index]->Update(g_engine->GetGPUDevice());
        }

        m_descriptor_sets_to_update[frame_index].Clear();

        m_descriptor_sets_to_update_flag.BitAnd(~(1u << frame_index), MemoryOrder::RELEASE);
    }
}

#pragma endregion MaterialDescriptorSetManager

namespace renderer {

HYP_DESCRIPTOR_SSBO_COND(Object, MaterialsBuffer, 1, sizeof(MaterialShaderData) * max_materials, false, !RenderConfig::ShouldCollectUniqueDrawCallPerMaterial());
HYP_DESCRIPTOR_SSBO_COND(Object, MaterialsBuffer, 1, sizeof(MaterialShaderData), true, RenderConfig::ShouldCollectUniqueDrawCallPerMaterial());

} // namespace renderer

} // namespace hyperion
