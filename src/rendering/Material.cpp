/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/Material.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <util/ByteUtil.hpp>

#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion {

using renderer::CommandBuffer;
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

                descriptor_set->SetElement(HYP_NAME(Textures), texture_index, texture->GetImageView());
            } else {
                descriptor_set->SetElement(HYP_NAME(Textures), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
            }
        }

        g_engine->GetMaterialDescriptorSetManager().SetNeedsDescriptorSetUpdate(id);

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
        .shader_definition = ShaderDefinition {
            HYP_NAME(Forward),
            static_mesh_vertex_attributes
        },
        .bucket = Bucket::BUCKET_OPAQUE
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
            HYP_NAME(Forward),
            static_mesh_vertex_attributes
        },
        .bucket = Bucket::BUCKET_OPAQUE
      },
      m_is_dynamic(false),
      m_mutation_state(DataMutationState::CLEAN)
{
    if (m_render_attributes.shader_definition) {
        m_shader = g_shader_manager->GetOrCreate(m_render_attributes.shader_definition);
    }

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
    if (m_render_attributes.shader_definition) {
        m_shader = g_shader_manager->GetOrCreate(m_render_attributes.shader_definition);
    }
}

Material::~Material()
{
    SetReady(false);

    for (SizeType i = 0; i < m_textures.Size(); i++) {
        m_textures.ValueAt(i).Reset();
    }

    g_safe_deleter->SafeReleaseHandle(std::move(m_shader));

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

    for (SizeType i = 0; i < m_textures.Size(); i++) {
        if (Handle<Texture> &texture = m_textures.ValueAt(i)) {
            DebugLog(
                LogType::Debug,
                "Material with ID %u: Init texture with ID %u, ImageViewRef index %u\n",
                GetID().Value(),
                texture->GetID().Value(),
                texture->GetImageView().index
            );

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

    g_engine->GetMaterialDescriptorSetManager().EnqueueAdd(GetID(), std::move(texture_bindings));
}

void Material::EnqueueDescriptorSetDestroy()
{
    g_engine->GetMaterialDescriptorSetManager().EnqueueRemove(GetID());
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

void Material::SetShader(Handle<Shader> shader)
{
    if (m_shader == shader) {
        return;
    }

    if (m_shader.IsValid()) {
        g_safe_deleter->SafeReleaseHandle(std::move(m_shader));
    }

    m_render_attributes.shader_definition = shader.IsValid()
        ? shader->GetCompiledShader().GetDefinition()
        : ShaderDefinition { };

    m_shader = std::move(shader);

    if (IsInitCalled()) {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::SetParameter(MaterialKey key, const Parameter &value)
{
    if (m_parameters[key] == value) {
        return;
    }

    m_parameters.Set(key, value);

    if (IsInitCalled()) {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::ResetParameters()
{
    m_parameters = DefaultParameters();

    if (IsInitCalled()) {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::SetTexture(TextureKey key, Handle<Texture> &&texture)
{
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
    auto handle = g_engine->CreateObject<Material>(
        m_name,
        m_render_attributes,
        m_parameters,
        m_textures
    );

    return handle;
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

    DebugLog(
        LogType::Debug,
        "Adding material with hash %u to material cache\n",
        hc.Value()
    );

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
            HYP_NAME(Forward),
            static_mesh_vertex_attributes
        };
    }

    auto handle = g_engine->CreateObject<Material>(
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
            HYP_NAME(Forward),
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
            DebugLog(
                LogType::Debug,
                "Reusing material with hash %u from material cache\n",
                hc.Value()
            );

            return handle;
        }
    }

    auto handle = g_engine->CreateObject<Material>(
        CreateNameFromDynamicString(ANSIString("cached_material_") + ANSIString::ToString(hc.Value())),
        attributes,
        parameters,
        textures
    );

    DebugLog(
        LogType::Debug,
        "Adding material with hash %u to material cache\n",
        hc.Value()
    );

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

    const renderer::DescriptorSetDeclaration *declaration = g_engine->GetGlobalDescriptorTable()->GetDeclaration().FindDescriptorSetDeclaration(HYP_NAME(Material));
    AssertThrow(declaration != nullptr);

    const renderer::DescriptorSetLayout layout(*declaration);

    const DescriptorSetRef invalid_descriptor_set = layout.CreateDescriptorSet();
    
    for (uint texture_index = 0; texture_index < max_bound_textures; texture_index++) {
        invalid_descriptor_set->SetElement(HYP_NAME(Textures), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
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

void MaterialDescriptorSetManager::EnqueueAdd(ID<Material> material)
{
    Mutex::Guard guard(m_pending_mutex);

    const renderer::DescriptorSetDeclaration *declaration = g_engine->GetGlobalDescriptorTable()->GetDeclaration().FindDescriptorSetDeclaration(HYP_NAME(Material));
    AssertThrow(declaration != nullptr);

    renderer::DescriptorSetLayout layout(*declaration);

    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        descriptor_sets[frame_index] = MakeRenderObject<renderer::DescriptorSet>(layout);

        for (uint texture_index = 0; texture_index < max_bound_textures; texture_index++) {
            descriptor_sets[frame_index]->SetElement(HYP_NAME(Textures), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }
    }

    m_pending_addition.PushBack({
        material,
        std::move(descriptor_sets)
    });

    m_pending_addition_flag.Set(true, MemoryOrder::RELAXED);
}

void MaterialDescriptorSetManager::EnqueueAdd(ID<Material> material, FixedArray<Handle<Texture>, max_bound_textures> &&textures)
{
    Mutex::Guard guard(m_pending_mutex);

    const renderer::DescriptorSetDeclaration *declaration = g_engine->GetGlobalDescriptorTable()->GetDeclaration().FindDescriptorSetDeclaration(HYP_NAME(Material));
    AssertThrow(declaration != nullptr);

    const renderer::DescriptorSetLayout layout(*declaration);

    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        descriptor_sets[frame_index] = layout.CreateDescriptorSet();

        for (uint texture_index = 0; texture_index < max_bound_textures; texture_index++) {
            if (texture_index < textures.Size()) {
                const Handle<Texture> &texture = textures[texture_index];

                if (texture.IsValid() && texture->GetImageView() != nullptr) {
                    descriptor_sets[frame_index]->SetElement(HYP_NAME(Textures), texture_index, texture->GetImageView());

                    continue;
                }
            }

            descriptor_sets[frame_index]->SetElement(HYP_NAME(Textures), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }
    }

    m_pending_addition.PushBack({
        material,
        std::move(descriptor_sets)
    });

    m_pending_addition_flag.Set(true, MemoryOrder::RELAXED);
}

void MaterialDescriptorSetManager::EnqueueRemove(ID<Material> material)
{
    Mutex::Guard guard(m_pending_mutex);
    
    while (true) {
        const auto pending_addition_it = m_pending_addition.FindIf([material](const auto &item)
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

    m_pending_addition_flag.Set(true, MemoryOrder::RELAXED);
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

    m_descriptor_sets_to_update_flag.Set(0x3, MemoryOrder::RELAXED);
}


void MaterialDescriptorSetManager::Initialize()
{
    CreateInvalidMaterialDescriptorSet();
}

void MaterialDescriptorSetManager::Update(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    const uint descriptor_sets_to_update_flag = m_descriptor_sets_to_update_flag.Get(MemoryOrder::RELAXED);

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

        m_descriptor_sets_to_update_flag.BitAnd(~(1u << frame_index), MemoryOrder::RELAXED);
    }


    if (!m_pending_addition_flag.Get(MemoryOrder::RELAXED)) {
        return;
    }

    Mutex::Guard guard(m_pending_mutex);

    for (auto it = m_pending_removal.Begin(); it != m_pending_removal.End();) {
        const auto material_descriptor_sets_it = m_material_descriptor_sets.Find(*it);

        if (material_descriptor_sets_it != m_material_descriptor_sets.End()) {
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

    m_pending_addition_flag.Set(false, MemoryOrder::RELAXED);
}

#pragma endregion MaterialDescriptorSetManager

} // namespace hyperion
