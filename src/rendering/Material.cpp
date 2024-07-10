/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Material.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <core/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/ByteUtil.hpp>

#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(Material);
HYP_DEFINE_CLASS(MaterialGroup);

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(UpdateMaterialRenderData) : renderer::RenderCommand
{
    ID<Material>                                id;
    MaterialShaderData                          shader_data;
    SizeType                                    num_bound_textures;
    FixedArray<ID<Texture>, max_bound_textures> bound_texture_ids;

    RENDER_COMMAND(UpdateMaterialRenderData)(
        ID<Material> id,
        const MaterialShaderData &shader_data,
        SizeType num_bound_textures,
        FixedArray<ID<Texture>, max_bound_textures> &&bound_texture_ids
    ) : id(id),
        shader_data(shader_data),
        num_bound_textures(num_bound_textures),
        bound_texture_ids(std::move(bound_texture_ids))
    {
    }

    virtual ~RENDER_COMMAND(UpdateMaterialRenderData)() override = default;

    virtual Result operator()() override
    {
        shader_data.texture_usage = 0;

        Memory::MemSet(shader_data.texture_index, 0, sizeof(shader_data.texture_index));

        static const bool use_bindless_textures = g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures();

        if (num_bound_textures != 0) {
            for (SizeType i = 0; i < bound_texture_ids.Size(); i++) {
                if (bound_texture_ids[i] != Texture::empty_id) {
                    if (use_bindless_textures) {
                        shader_data.texture_index[i] = bound_texture_ids[i].ToIndex();
                    } else {
                        shader_data.texture_index[i] = i;
                    }

                    shader_data.texture_usage |= 1 << i;

                    if (i + 1 == num_bound_textures) {
                        break;
                    }
                }
            }
        }

        g_engine->GetRenderData()->materials.Set(id.ToIndex(), shader_data);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateMaterialTexture) : renderer::RenderCommand
{
    ID<Material>    id;
    SizeType        texture_index;
    Handle<Texture> texture;

    RENDER_COMMAND(UpdateMaterialTexture)(
        ID<Material> id,
        SizeType texture_index,
        Handle<Texture> texture
    ) : id(id),
        texture_index(texture_index),
        texture(std::move(texture))
    {
    }

    virtual ~RENDER_COMMAND(UpdateMaterialTexture)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSetRef &descriptor_set = g_engine->GetMaterialDescriptorSetManager().GetDescriptorSet(id, frame_index);
            AssertThrow(descriptor_set != nullptr);

            if (texture.IsValid()) {
                AssertThrow(texture->GetImageView() != nullptr);

                descriptor_set->SetElement(NAME("Textures"), texture_index, texture->GetImageView());
            } else {
                descriptor_set->SetElement(NAME("Textures"), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
            }
        }

        g_engine->GetMaterialDescriptorSetManager().SetNeedsDescriptorSetUpdate(id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveMaterialDescriptorSet) : renderer::RenderCommand
{
    ID<Material>    id;

    RENDER_COMMAND(RemoveMaterialDescriptorSet)(ID<Material> id)
        : id(id)
    {
    }

    virtual ~RENDER_COMMAND(RemoveMaterialDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        g_engine->GetMaterialDescriptorSetManager().RemoveMaterial(id);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region Material

Material::ParameterTable Material::DefaultParameters()
{
    ParameterTable parameters;

    parameters.Set(MATERIAL_KEY_ALBEDO,               Vector4(1.0f));
    parameters.Set(MATERIAL_KEY_METALNESS,            0.0f);
    parameters.Set(MATERIAL_KEY_ROUGHNESS,            0.65f);
    parameters.Set(MATERIAL_KEY_TRANSMISSION,         0.0f);
    parameters.Set(MATERIAL_KEY_EMISSIVE,             0.0f);
    parameters.Set(MATERIAL_KEY_SPECULAR,             0.0f);
    parameters.Set(MATERIAL_KEY_SPECULAR_TINT,        0.0f);
    parameters.Set(MATERIAL_KEY_ANISOTROPIC,          0.0f);
    parameters.Set(MATERIAL_KEY_SHEEN,                0.0f);
    parameters.Set(MATERIAL_KEY_SHEEN_TINT,           0.0f);
    parameters.Set(MATERIAL_KEY_CLEARCOAT,            0.0f);
    parameters.Set(MATERIAL_KEY_CLEARCOAT_GLOSS,      0.0f);
    parameters.Set(MATERIAL_KEY_SUBSURFACE,           0.0f);
    parameters.Set(MATERIAL_KEY_NORMAL_MAP_INTENSITY, 1.0f);
    parameters.Set(MATERIAL_KEY_UV_SCALE,             Vector2(1.0f));
    parameters.Set(MATERIAL_KEY_PARALLAX_HEIGHT,      0.05f);
    parameters.Set(MATERIAL_KEY_ALPHA_THRESHOLD,      0.2f);

    return parameters;
}

Material::Material()
    : BasicObject(),
      m_render_attributes {
        .shader_definition  = ShaderDefinition { NAME("Forward"), static_mesh_vertex_attributes },
        .bucket             = Bucket::BUCKET_OPAQUE,
        .fill_mode          = FillMode::FILL,
        .blend_function     = BlendFunction::None(),
        .cull_faces         = FaceCullMode::BACK,
        .flags              = MaterialAttributeFlags::DEPTH_WRITE | MaterialAttributeFlags::DEPTH_TEST
      },
      m_is_dynamic(false),
      m_mutation_state(DataMutationState::CLEAN)
{
    ResetParameters();
}

Material::Material(Name name, Bucket bucket)
    : BasicObject(name),
      m_render_attributes {
        .shader_definition = ShaderDefinition {
            NAME("Forward"),
            static_mesh_vertex_attributes
        },
        .bucket = Bucket::BUCKET_OPAQUE
      },
      m_is_dynamic(false),
      m_mutation_state(DataMutationState::CLEAN)
{
    ResetParameters();
}

Material::Material(
    Name name,
    const MaterialAttributes &attributes,
    const ParameterTable &parameters,
    const TextureSet &textures
) : BasicObject(name),
    m_parameters(parameters),
    m_textures(textures),
    m_render_attributes(attributes),
    m_is_dynamic(false),
    m_mutation_state(DataMutationState::CLEAN)
{
}

Material::~Material()
{
    SetReady(false);

    for (SizeType i = 0; i < m_textures.Size(); i++) {
        m_textures.ValueAt(i).Reset();
    }

    g_safe_deleter->SafeRelease(std::move(m_shader));

    if (IsInitCalled()) {
        if (!g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            EnqueueDescriptorSetDestroy();
        }

        HYP_SYNC_RENDER();
    }
}

void Material::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    if (!m_shader.IsValid()) {
        if (m_render_attributes.shader_definition) {
            m_shader = g_shader_manager->GetOrCreate(m_render_attributes.shader_definition);
        }

        if (!m_shader.IsValid()) {
            HYP_LOG(Material, LogLevel::ERR, "Failed to create shader for material with ID #{} (name: {})", GetID().Value(), GetName());
        }
    }

    for (SizeType i = 0; i < m_textures.Size(); i++) {
        if (Handle<Texture> &texture = m_textures.ValueAt(i)) {
            InitObject(texture);
        }
    }
    
    if (!g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
        EnqueueDescriptorSetCreate();
    }

    m_mutation_state |= DataMutationState::DIRTY;

    SetReady(true);

    EnqueueRenderUpdates();
}

void Material::EnqueueDescriptorSetCreate()
{
    HYP_LOG(Material, LogLevel::DEBUG, "EnqueueCreate material descriptor set for material with ID #{} (name: {})", GetID().Value(), GetName());

    FixedArray<Handle<Texture>, max_bound_textures> texture_bindings;

    for (const Pair<TextureKey, Handle<Texture> &> it : m_textures) {
        const SizeType texture_index = decltype(m_textures)::EnumToOrdinal(it.first);

        if (texture_index >= texture_bindings.Size()) {
            continue;
        }

        if (const Handle<Texture> &texture = it.second) {
            texture_bindings[texture_index] = texture;
        }
    }

    g_engine->GetMaterialDescriptorSetManager().AddMaterial(GetID(), std::move(texture_bindings));
}

void Material::EnqueueDescriptorSetDestroy()
{
    // g_engine->GetMaterialDescriptorSetManager().EnqueueRemoveMaterial(GetID());

    PUSH_RENDER_COMMAND(
        RemoveMaterialDescriptorSet,
        GetID()
    );
}

void Material::EnqueueRenderUpdates()
{
    AssertReady();

    if (!m_mutation_state.IsDirty()) {
        return;
    }

    FixedArray<ID<Texture>, max_bound_textures> bound_texture_ids { };

    static const uint num_bound_textures = MathUtil::Min(
        max_textures,
        g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()
            ? max_bindless_resources
            : max_bound_textures
    );

    for (uint i = 0; i < num_bound_textures; i++) {
        if (const Handle<Texture> &texture = m_textures.ValueAt(i)) {
            bound_texture_ids[i] = texture->GetID();
        }
    }

    MaterialShaderData shader_data {
        .albedo = GetParameter<Vec4f>(MATERIAL_KEY_ALBEDO),
        .packed_params = Vec4u(
            ByteUtil::PackVec4f(Vec4f(
                GetParameter<float>(MATERIAL_KEY_ROUGHNESS),
                GetParameter<float>(MATERIAL_KEY_METALNESS),
                GetParameter<float>(MATERIAL_KEY_TRANSMISSION),
                GetParameter<float>(MATERIAL_KEY_NORMAL_MAP_INTENSITY)
            )),
            ByteUtil::PackVec4f(Vec4f(
                GetParameter<float>(MATERIAL_KEY_ALPHA_THRESHOLD)
            )),
            ByteUtil::PackVec4f(Vec4f { }),
            ByteUtil::PackVec4f(Vec4f { })
        ),
        .uv_scale = GetParameter<Vec2f>(MATERIAL_KEY_UV_SCALE),
        .parallax_height = GetParameter<float>(MATERIAL_KEY_PARALLAX_HEIGHT)
    };

    PUSH_RENDER_COMMAND(
        UpdateMaterialRenderData,
        GetID(),
        shader_data,
        num_bound_textures,
        std::move(bound_texture_ids)
    );

    m_mutation_state = DataMutationState::CLEAN;
}

void Material::EnqueueTextureUpdate(TextureKey key)
{
    const SizeType texture_index = decltype(m_textures)::EnumToOrdinal(key);

    const Handle<Texture> &texture = m_textures.Get(key);
    AssertThrow(texture.IsValid());

    PUSH_RENDER_COMMAND(
        UpdateMaterialTexture,
        m_id,
        texture_index,
        texture
    );
}

void Material::SetShader(const ShaderRef &shader)
{
    if (IsStatic()) {
        HYP_LOG(Material, LogLevel::WARNING, "Setting shader on static material with ID #{} (name: {})", GetID().Value(), GetName());
    }

    if (m_shader == shader) {
        return;
    }

    if (m_shader.IsValid()) {
        SafeRelease(std::move(m_shader));
    }

    m_render_attributes.shader_definition = shader.IsValid()
        ? shader->GetCompiledShader()->GetDefinition()
        : ShaderDefinition { };

    if (!bool(m_render_attributes.shader_definition)) {
        HYP_BREAKPOINT;
    }

    m_shader = shader;

    if (IsInitCalled()) {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::SetParameter(MaterialKey key, const Parameter &value)
{
    if (IsStatic()) {
        HYP_LOG(Material, LogLevel::WARNING, "Setting parameter on static material with ID #{} (name: {})", GetID().Value(), GetName());
    }

    if (m_parameters[key] == value) {
        return;
    }

    m_parameters.Set(key, value);

    if (IsInitCalled()) {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::SetParameters(const ParameterTable &parameters)
{
    if (IsStatic()) {
        HYP_LOG(Material, LogLevel::WARNING, "Setting parameters on static material with ID #{} (name: {})", GetID().Value(), GetName());
    }

    m_parameters = parameters;

    if (IsInitCalled()) {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::ResetParameters()
{
    if (IsStatic()) {
        HYP_LOG(Material, LogLevel::WARNING, "Resetting parameters on static material with ID #{} (name: {})", GetID().Value(), GetName());
    }

    m_parameters = DefaultParameters();

    if (IsInitCalled()) {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::SetTexture(TextureKey key, Handle<Texture> &&texture)
{
    if (IsStatic()) {
        HYP_LOG(Material, LogLevel::WARNING, "Setting texture on static material with ID #{} (name: {})", GetID().Value(), GetName());
    }

    if (m_textures[key] == texture) {
        return;
    }

    m_textures.Set(key, texture);
    
    if (IsInitCalled()) {
        InitObject(texture);

        if (!g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            EnqueueTextureUpdate(key);
        }

        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::SetTexture(TextureKey key, const Handle<Texture> &texture)
{
    SetTexture(key, Handle<Texture>(texture));
}

void Material::SetTextureAtIndex(uint index, const Handle<Texture> &texture)
{
    return SetTexture(m_textures.KeyAt(index), texture);
}

void Material::SetTextures(const TextureSet &textures)
{
    // @TODO : only update textures that have changed
    
    if (IsStatic()) {
        HYP_LOG(Material, LogLevel::WARNING, "Setting textures on static material with ID #{} (name: {})", GetID().Value(), GetName());
    }

    m_textures = textures;

    if (IsInitCalled()) {
        for (SizeType i = 0; i < m_textures.Size(); i++) {
            if (!m_textures.ValueAt(i).IsValid()) {
                continue;
            }

            InitObject(m_textures.ValueAt(i));
        }

        if (!g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            for (SizeType i = 0; i < m_textures.Size(); i++) {
                if (!m_textures.ValueAt(i).IsValid()) {
                    continue;
                }
                
                EnqueueTextureUpdate(m_textures.KeyAt(i));
            }
        }

        m_mutation_state |= DataMutationState::DIRTY;
    }
}

const Handle<Texture> &Material::GetTexture(TextureKey key) const
{
    return m_textures.Get(key);
}

const Handle<Texture> &Material::GetTextureAtIndex(uint index) const
{
    return GetTexture(m_textures.KeyAt(index));
}

Handle<Material> Material::Clone() const
{
    Handle<Material> material = CreateObject<Material>(
        Name::Unique(*m_name),
        m_render_attributes,
        m_parameters,
        m_textures
    );

    // cloned materials are dynamic by default
    material->m_is_dynamic = true;

    return material;
}

#pragma endregion Material

#pragma region MaterialGroup

MaterialGroup::MaterialGroup()
    : BasicObject()
{
}

MaterialGroup::~MaterialGroup()
{
}

void MaterialGroup::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    for (auto &it : m_materials) {
        InitObject(it.second);
    }
}

void MaterialGroup::Add(const String &name, Handle<Material> &&material)
{
    if (IsInitCalled()) {
        InitObject(material);
    }

    m_materials[name] = std::move(material);
}

bool MaterialGroup::Remove(const String &name)
{
    const auto it = m_materials.Find(name);

    if (it != m_materials.End()) {
        m_materials.Erase(it);

        return true;
    }

    return false;
}

#pragma endregion MaterialGroup

#pragma region MaterialCache

MaterialCache *MaterialCache::GetInstance()
{
    return g_material_system;
}

void MaterialCache::Add(const Handle<Material> &material)
{
    if (!material) {
        return;
    }

    Mutex::Guard guard(m_mutex);

    HashCode hc;
    hc.Add(material->GetRenderAttributes().GetHashCode());
    hc.Add(material->GetParameters().GetHashCode());
    hc.Add(material->GetTextures().GetHashCode());

    HYP_LOG(Material, LogLevel::WARNING, "Adding material with hash {} to material cache", hc.Value());

    m_map.Set(hc.Value(), material);
}

Handle<Material> MaterialCache::CreateMaterial(
    MaterialAttributes attributes,
    const Material::ParameterTable &parameters,
    const Material::TextureSet &textures
)
{
    if (!attributes.shader_definition) {
        attributes.shader_definition = ShaderDefinition {
            NAME("Forward"),
            static_mesh_vertex_attributes
        };
    }

    Handle<Material> handle = CreateObject<Material>(
        Name::Unique("material"),
        attributes,
        parameters,
        textures
    );

    InitObject(handle);

    return handle;
}

Handle<Material> MaterialCache::GetOrCreate(
    MaterialAttributes attributes,
    const Material::ParameterTable &parameters,
    const Material::TextureSet &textures
)
{
    if (!attributes.shader_definition) {
        attributes.shader_definition = ShaderDefinition {
            NAME("Forward"),
            static_mesh_vertex_attributes
        };
    }

    // @TODO: For textures hashcode, asset path should be used rather than texture ID

    HashCode hc;
    hc.Add(attributes.GetHashCode());
    hc.Add(parameters.GetHashCode());
    hc.Add(textures.GetHashCode());

    Mutex::Guard guard(m_mutex);

    const auto it = m_map.Find(hc.Value());

    if (it != m_map.End()) {
        if (Handle<Material> handle = it->second.Lock()) {
            HYP_LOG(Material, LogLevel::INFO, "Reusing material with hash {} from material cache", hc.Value());

            return handle;
        }
    }

    Handle<Material> handle = CreateObject<Material>(
        CreateNameFromDynamicString(ANSIString("cached_material_") + ANSIString::ToString(hc.Value())),
        attributes,
        parameters,
        textures
    );

    HYP_LOG(Material, LogLevel::DEBUG, "Adding material with hash {} to material cache", hc.Value());

    InitObject(handle);

    m_map.Set(hc.Value(), handle);

    return handle;
}

#pragma region MaterialCache

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
    
    for (uint texture_index = 0; texture_index < max_bound_textures; texture_index++) {
        invalid_descriptor_set->SetElement(NAME("Textures"), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
    }

    DeferCreate(invalid_descriptor_set, g_engine->GetGPUDevice());

    m_material_descriptor_sets.Set(ID<Material>::invalid, { invalid_descriptor_set, invalid_descriptor_set });
}

const DescriptorSetRef &MaterialDescriptorSetManager::GetDescriptorSet(ID<Material> material, uint frame_index) const
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);

    const auto it = m_material_descriptor_sets.Find(material);

    if (it == m_material_descriptor_sets.End()) {
        return DescriptorSetRef::unset;
    }

    return it->second[frame_index];
}

void MaterialDescriptorSetManager::AddMaterial(ID<Material> id)
{
    const renderer::DescriptorSetDeclaration *declaration = g_engine->GetGlobalDescriptorTable()->GetDeclaration().FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(declaration != nullptr);

    renderer::DescriptorSetLayout layout(*declaration);

    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        descriptor_sets[frame_index] = MakeRenderObject<DescriptorSet>(layout);

        for (uint texture_index = 0; texture_index < max_bound_textures; texture_index++) {
            descriptor_sets[frame_index]->SetElement(NAME("Textures"), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }
    }

    // if on render thread, initialize and add immediately
    if (Threads::IsOnThread(ThreadName::THREAD_RENDER)) {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_ASSERT_RESULT(descriptor_sets[frame_index]->Create(g_engine->GetGPUDevice()));
        }

        m_material_descriptor_sets.Insert(id, std::move(descriptor_sets));

        return;
    }

    Mutex::Guard guard(m_pending_mutex);

    m_pending_addition.PushBack({
        id,
        std::move(descriptor_sets)
    });

    m_pending_addition_flag.Set(true, MemoryOrder::RELEASE);
}

void MaterialDescriptorSetManager::AddMaterial(ID<Material> id, FixedArray<Handle<Texture>, max_bound_textures> &&textures)
{
    const renderer::DescriptorSetDeclaration *declaration = g_engine->GetGlobalDescriptorTable()->GetDeclaration().FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(declaration != nullptr);

    const renderer::DescriptorSetLayout layout(*declaration);

    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        descriptor_sets[frame_index] = layout.CreateDescriptorSet();

        for (uint texture_index = 0; texture_index < max_bound_textures; texture_index++) {
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
    if (Threads::IsOnThread(ThreadName::THREAD_RENDER)) {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_ASSERT_RESULT(descriptor_sets[frame_index]->Create(g_engine->GetGPUDevice()));
        }

        m_material_descriptor_sets.Insert(id, std::move(descriptor_sets));

        return;
    }

    Mutex::Guard guard(m_pending_mutex);

    m_pending_addition.PushBack({
        id,
        std::move(descriptor_sets)
    });

    m_pending_addition_flag.Set(true, MemoryOrder::RELEASE);
}

void MaterialDescriptorSetManager::EnqueueRemoveMaterial(ID<Material> id)
{
    HYP_LOG(Material, LogLevel::DEBUG, "EnqueueRemove material with ID {} from thread {}", id.Value(), Threads::CurrentThreadID().name);

    Mutex::Guard guard(m_pending_mutex);
    
    while (true) {
        const auto pending_addition_it = m_pending_addition.FindIf([id](const auto &item)
            {
                return item.first == id;
            });

        if (pending_addition_it == m_pending_addition.End()) {
            break;
        }

        m_pending_addition.Erase(pending_addition_it);
    }

    if (!m_pending_removal.Contains(id)) {
        m_pending_removal.PushBack(id);
    }

    m_pending_addition_flag.Set(true, MemoryOrder::RELEASE);
}

void MaterialDescriptorSetManager::RemoveMaterial(ID<Material> id)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    { // remove from pending
        Mutex::Guard guard(m_pending_mutex);
    
        while (true) {
            const auto pending_addition_it = m_pending_addition.FindIf([id](const auto &item)
                {
                    return item.first == id;
                });

            if (pending_addition_it == m_pending_addition.End()) {
                break;
            }

            m_pending_addition.Erase(pending_addition_it);
        }

        const auto pending_removal_it = m_pending_removal.Find(id);

        if (pending_removal_it != m_pending_removal.End()) {
            m_pending_removal.Erase(pending_removal_it);
        }
    }

    const auto material_descriptor_sets_it = m_material_descriptor_sets.Find(id);

    if (material_descriptor_sets_it != m_material_descriptor_sets.End()) {
        HYP_LOG(Material, LogLevel::DEBUG, "Releasing descriptor sets for material with ID {} from thread {}", id.Value(), Threads::CurrentThreadID().name);

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            SafeRelease(std::move(material_descriptor_sets_it->second[frame_index]));
        }

        m_material_descriptor_sets.Erase(material_descriptor_sets_it);
    }
}

void MaterialDescriptorSetManager::SetNeedsDescriptorSetUpdate(ID<Material> id)
{
    Mutex::Guard guard(m_descriptor_sets_to_update_mutex);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const auto it = m_descriptor_sets_to_update[frame_index].Find(id);

        if (it != m_descriptor_sets_to_update[frame_index].End()) {
            continue;
        }

        m_descriptor_sets_to_update[frame_index].PushBack(id);
    }

    m_descriptor_sets_to_update_flag.Set(0x3, MemoryOrder::RELEASE);
}


void MaterialDescriptorSetManager::Initialize()
{
    CreateInvalidMaterialDescriptorSet();
}

void MaterialDescriptorSetManager::Update(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    const uint descriptor_sets_to_update_flag = m_descriptor_sets_to_update_flag.Get(MemoryOrder::ACQUIRE);

    if (descriptor_sets_to_update_flag & (1u << frame_index)) {
        Mutex::Guard guard(m_descriptor_sets_to_update_mutex);

        for (ID<Material> id : m_descriptor_sets_to_update[frame_index]) {
            const auto it = m_material_descriptor_sets.Find(id);

            if (it == m_material_descriptor_sets.End()) {
                continue;
            }

            AssertThrow(it->second[frame_index].IsValid());
            it->second[frame_index]->Update(g_engine->GetGPUDevice());
        }

        m_descriptor_sets_to_update[frame_index].Clear();

        m_descriptor_sets_to_update_flag.BitAnd(~(1u << frame_index), MemoryOrder::RELEASE);
    }

    if (!m_pending_addition_flag.Get(MemoryOrder::ACQUIRE)) {
        return;
    }

    Mutex::Guard guard(m_pending_mutex);

    for (auto it = m_pending_removal.Begin(); it != m_pending_removal.End();) {
        const auto material_descriptor_sets_it = m_material_descriptor_sets.Find(*it);

        if (material_descriptor_sets_it != m_material_descriptor_sets.End()) {
            HYP_LOG(Material, LogLevel::DEBUG, "Releasing descriptor sets for material with ID {} from thread {}", it->Value(), Threads::CurrentThreadID().name);

            for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                SafeRelease(std::move(material_descriptor_sets_it->second[frame_index]));
            }

            m_material_descriptor_sets.Erase(material_descriptor_sets_it);
        }

        it = m_pending_removal.Erase(it);
    }

    for (auto it = m_pending_addition.Begin(); it != m_pending_addition.End();) {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            AssertThrow(it->second[frame_index].IsValid());

            HYPERION_ASSERT_RESULT(it->second[frame_index]->Create(g_engine->GetGPUDevice()));
        }

        m_material_descriptor_sets.Insert(it->first, std::move(it->second));

        it = m_pending_addition.Erase(it);
    }

    m_pending_addition_flag.Set(false, MemoryOrder::RELEASE);
}

#pragma endregion MaterialDescriptorSetManager

} // namespace hyperion
