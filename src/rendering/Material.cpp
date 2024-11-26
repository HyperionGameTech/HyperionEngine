/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Material.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/ByteUtil.hpp>

#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion {

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(UpdateMaterialRenderData) : renderer::RenderCommand
{
    WeakHandle<Material>                        material;
    MaterialShaderData                          shader_data;
    SizeType                                    num_bound_textures;
    FixedArray<ID<Texture>, max_bound_textures> bound_texture_ids;

    RENDER_COMMAND(UpdateMaterialRenderData)(
        const WeakHandle<Material> &material,
        const MaterialShaderData &shader_data,
        SizeType num_bound_textures,
        FixedArray<ID<Texture>, max_bound_textures> &&bound_texture_ids
    ) : material(material),
        shader_data(shader_data),
        num_bound_textures(num_bound_textures),
        bound_texture_ids(std::move(bound_texture_ids))
    {
    }

    virtual ~RENDER_COMMAND(UpdateMaterialRenderData)() override = default;

    virtual Result operator()() override
    {
        if (Handle<Material> material_locked = material.Lock()) {
            shader_data.texture_usage = 0;

            Memory::MemSet(shader_data.texture_index, 0, sizeof(shader_data.texture_index));

            static const bool use_bindless_textures = g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures();

            if (num_bound_textures != 0) {
                for (SizeType i = 0; i < bound_texture_ids.Size(); i++) {
                    if (bound_texture_ids[i] != ID<Texture>::invalid) {
                        if (use_bindless_textures) {
                            shader_data.texture_index[i] = bound_texture_ids[i].ToIndex();
                        } else {
                            shader_data.texture_index[i] = i;
                        }

                        shader_data.texture_usage |= 1 << i;
                    }
                }
            }

            g_engine->GetRenderData()->materials->Set(material.GetID().ToIndex(), shader_data);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateMaterialTexture) : renderer::RenderCommand
{
    WeakHandle<Material>    material;
    SizeType                texture_index;
    Handle<Texture>         texture;

    RENDER_COMMAND(UpdateMaterialTexture)(
        const Handle<Material> &material,
        SizeType texture_index,
        const Handle<Texture> &texture
    ) : material(material),
        texture_index(texture_index),
        texture(texture)
    {
        AssertThrow(material.IsValid());
        AssertThrow(material->IsReady());
    }

    virtual ~RENDER_COMMAND(UpdateMaterialTexture)() override = default;

    virtual Result operator()() override
    {
        if (Handle<Material> material_locked = material.Lock()) {
            for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                const DescriptorSetRef &descriptor_set = g_engine->GetMaterialDescriptorSetManager().GetDescriptorSet(material.GetID(), frame_index);
                AssertThrow(descriptor_set != nullptr);

                if (texture.IsValid()) {
                    AssertThrow(texture->GetImageView() != nullptr);

                    descriptor_set->SetElement(NAME("Textures"), texture_index, texture->GetImageView());
                } else {
                    descriptor_set->SetElement(NAME("Textures"), texture_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
                }
            }

            g_engine->GetMaterialDescriptorSetManager().SetNeedsDescriptorSetUpdate(material_locked);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateMaterialDescriptorSet) : renderer::RenderCommand
{
    WeakHandle<Material>                            material;
    FixedArray<Handle<Texture>, max_bound_textures> textures;

    RENDER_COMMAND(CreateMaterialDescriptorSet)(
        const Handle<Material> &material,
        FixedArray<Handle<Texture>, max_bound_textures> &&textures
    ) : material(material),
        textures(std::move(textures))
    {
        AssertThrow(material.IsValid());
        AssertThrow(material->IsReady());
    }

    virtual ~RENDER_COMMAND(CreateMaterialDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        if (Handle<Material> material_locked = material.Lock()) {
            g_engine->GetMaterialDescriptorSetManager().AddMaterial(material_locked, std::move(textures));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveMaterialDescriptorSet) : renderer::RenderCommand
{
    WeakHandle<Material>    material;

    RENDER_COMMAND(RemoveMaterialDescriptorSet)(const Handle<Material> &material)
        : material(material)
    {
        AssertThrow(material.IsValid());
        AssertThrow(material->IsReady());
    }

    virtual ~RENDER_COMMAND(RemoveMaterialDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        g_engine->GetMaterialDescriptorSetManager().RemoveMaterial(material.GetID());

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region MaterialRenderResources

MaterialRenderResources::MaterialRenderResources(const WeakHandle<Material> &material_weak)
    : m_material_weak(material_weak)
{
}

MaterialRenderResources::~MaterialRenderResources()
{
    if (m_material_weak.IsValid()) {
        if (!g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            DestroyDescriptorSets();
        }
    }
}

void MaterialRenderResources::SetTexture(MaterialTextureKey texture_key, const Handle<Texture> &texture)
{
    Mutex::Guard guard(m_mutex);

    m_textures.Set(texture_key, texture);

    SetNeedsUpdate();
}

void MaterialRenderResources::SetTextures(FlatMap<MaterialTextureKey, Handle<Texture>> &&textures)
{
    Mutex::Guard guard(m_mutex);

    m_textures = std::move(textures);

    SetNeedsUpdate();
}

void MaterialRenderResources::Initialize()
{
    AssertThrow(m_material_weak.IsValid());

    if (Handle<Material> material = m_material_weak.Lock()) {
        if (!g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            CreateDescriptorSets();
        }
    }
}

void MaterialRenderResources::Destroy()
{
    AssertThrow(m_material_weak.IsValid());

    if (Handle<Material> material = m_material_weak.Lock()) {
        if (!g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            DestroyDescriptorSets();
        }
    }
}

void MaterialRenderResources::Update()
{
    AssertThrow(m_material_weak.IsValid());

    if (Handle<Material> material = m_material_weak.Lock()) {
        if (!g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            Mutex::Guard guard(m_mutex);

            for (const auto &it : m_textures) {
                const SizeType texture_index = Material::TextureSet::EnumToOrdinal(it.first);

                if (texture_index >= max_bound_textures) {
                    HYP_LOG(Material, LogLevel::WARNING, "Texture index {} is out of bounds of max bound textures ({})", texture_index, max_bound_textures);

                    continue;
                }

                const Handle<Texture> &texture = it.second;

                PUSH_RENDER_COMMAND(
                    UpdateMaterialTexture,
                    material,
                    texture_index,
                    texture
                );
            }
        }
    }
}

void MaterialRenderResources::CreateDescriptorSets()
{
    if (Handle<Material> material = m_material_weak.Lock()) {
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

        PUSH_RENDER_COMMAND(
            CreateMaterialDescriptorSet,
            material,
            std::move(texture_bindings)
        );
    }
}

void MaterialRenderResources::DestroyDescriptorSets()
{
    if (!m_material_weak.IsValid()) {
        return;
    }
    
    if (Handle<Material> material = m_material_weak.Lock()) {
        PUSH_RENDER_COMMAND(
            RemoveMaterialDescriptorSet,
            material
        );
    }
}

#pragma endregion MaterialRenderResources

#pragma region Material

const Material::ParameterTable &Material::DefaultParameters()
{
    static const ParameterTable parameters {
        { MATERIAL_KEY_ALBEDO, Vec4f(1.0f) },
        { MATERIAL_KEY_METALNESS, 0.0f },
        { MATERIAL_KEY_ROUGHNESS, 0.65f },
        { MATERIAL_KEY_TRANSMISSION, 0.0f },
        { MATERIAL_KEY_EMISSIVE, 0.0f },
        { MATERIAL_KEY_SPECULAR, 0.0f },
        { MATERIAL_KEY_SPECULAR_TINT, 0.0f },
        { MATERIAL_KEY_ANISOTROPIC, 0.0f },
        { MATERIAL_KEY_SHEEN, 0.0f },
        { MATERIAL_KEY_SHEEN_TINT, 0.0f },
        { MATERIAL_KEY_CLEARCOAT, 0.0f },
        { MATERIAL_KEY_CLEARCOAT_GLOSS, 0.0f },
        { MATERIAL_KEY_SUBSURFACE, 0.0f },
        { MATERIAL_KEY_NORMAL_MAP_INTENSITY, 1.0f },
        { MATERIAL_KEY_UV_SCALE, Vec2f(1.0f) },
        { MATERIAL_KEY_PARALLAX_HEIGHT, 0.05f },
        { MATERIAL_KEY_ALPHA_THRESHOLD, 0.2f }
    };

    return parameters;
}

Material::Material()
    : m_render_attributes {
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
    : m_name(name),
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
) : m_name(name),
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
}

void Material::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    m_render_resources = MakeRefCountedPtr<MaterialRenderResources>(WeakHandleFromThis());

    if (!m_shader.IsValid()) {
        if (m_render_attributes.shader_definition) {
            m_shader = g_shader_manager->GetOrCreate(m_render_attributes.shader_definition);
        }

        if (!m_shader.IsValid()) {
            HYP_LOG(Material, LogLevel::ERR, "Failed to create shader for material with ID #{} (name: {})", GetID().Value(), GetName());
        }
    }

    FlatMap<MaterialTextureKey, Handle<Texture>> textures;

    for (SizeType i = 0; i < m_textures.Size(); i++) {
        MaterialTextureKey key = m_textures.KeyAt(i);

        if (Handle<Texture> &texture = m_textures.ValueAt(i)) {
            InitObject(texture);

            textures.Set(key, texture);
        }
    }

    m_render_resources->SetTextures(std::move(textures));

    m_mutation_state |= DataMutationState::DIRTY;

    SetReady(true);

    EnqueueRenderUpdates();
}

void Material::EnqueueRenderUpdates()
{
    AssertReady();

    if (!m_mutation_state.IsDirty()) {
        HYP_LOG_ONCE(Material, LogLevel::WARNING, "EnqueueRenderUpdates called on material with ID #{} (name: {}) that is not dirty", GetID().Value(), GetName());

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
        WeakHandleFromThis(),
        shader_data,
        num_bound_textures,
        std::move(bound_texture_ids)
    );

    m_mutation_state = DataMutationState::CLEAN;
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

void Material::SetTexture(MaterialTextureKey key, const Handle<Texture> &texture)
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

        m_render_resources->SetTexture(key, texture);

        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::SetTextureAtIndex(uint index, const Handle<Texture> &texture)
{
    return SetTexture(m_textures.KeyAt(index), texture);
}

void Material::SetTextures(const TextureSet &textures)
{
    if (IsStatic()) {
        HYP_LOG(Material, LogLevel::WARNING, "Setting textures on static material with ID #{} (name: {})", GetID().Value(), GetName());
    }

    if (m_textures == textures) {
        return;
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
            FlatMap<MaterialTextureKey, Handle<Texture>> textures;

            for (SizeType i = 0; i < m_textures.Size(); i++) {
                MaterialTextureKey key = m_textures.KeyAt(i);

                if (Handle<Texture> &texture = m_textures.ValueAt(i)) {
                    textures.Set(key, texture);
                }
            }

            m_render_resources->SetTextures(std::move(textures));
        }

        m_mutation_state |= DataMutationState::DIRTY;
    }
}

const Handle<Texture> &Material::GetTexture(MaterialTextureKey key) const
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
        m_name,
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

    AssertThrowMsg(!material->IsDynamic(), "Cannot add dynamic material to cache, as changes to the material will affect all instances");

    Mutex::Guard guard(m_mutex);

    HashCode hc;
    hc.Add(material->GetRenderAttributes().GetHashCode());
    hc.Add(material->GetParameters().GetHashCode());
    hc.Add(material->GetTextures().GetHashCode());

    HYP_LOG(Material, LogLevel::WARNING, "Adding material with hash {} to material cache", hc.Value());

    m_map.Set(hc, material);
}

Handle<Material> MaterialCache::CreateMaterial(
    Name name,
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
        name,
        attributes,
        parameters,
        textures
    );

    InitObject(handle);

    return handle;
}

Handle<Material> MaterialCache::GetOrCreate(
    Name name,
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
    // textures may later be destroyed and their IDs reused which would cause a hash collision

    HashCode hc;
    hc.Add(attributes.GetHashCode());
    hc.Add(parameters.GetHashCode());
    hc.Add(textures.GetHashCode());

    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_map.Find(hc);

        if (it != m_map.End()) {
            if (Handle<Material> handle = it->second.Lock()) {
                HYP_LOG(Material, LogLevel::INFO, "Reusing material with hash {} from material cache", hc.Value());

                return handle;
            }
        }
    }
    
    if (!name.IsValid()) {
        name = CreateNameFromDynamicString(ANSIString("cached_material_") + ANSIString::ToString(hc.Value()));
    }

    Handle<Material> handle = CreateObject<Material>(
        name,
        attributes,
        parameters,
        textures
    );

    AssertThrow(!handle->IsDynamic());

    HYP_LOG(Material, LogLevel::INFO, "Adding material with hash {} to material cache", hc.Value());

    InitObject(handle);

    {
        Mutex::Guard guard(m_mutex);
        
        m_map.Set(hc, handle);
    }

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

    m_material_descriptor_sets.Set(WeakHandle<Material> {}, { invalid_descriptor_set, invalid_descriptor_set });
}

const DescriptorSetRef &MaterialDescriptorSetManager::GetDescriptorSet(ID<Material> id, uint frame_index) const
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);

    const auto it = m_material_descriptor_sets.FindAs(id);

    if (it == m_material_descriptor_sets.End()) {
        return DescriptorSetRef::unset;
    }

    return it->second[frame_index];
}

void MaterialDescriptorSetManager::AddMaterial(const Handle<Material> &material)
{
    if (!material.IsValid()) {
        return;
    }

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

        m_material_descriptor_sets.Insert(material, std::move(descriptor_sets));

        return;
    }

    Mutex::Guard guard(m_pending_mutex);

    m_pending_addition.PushBack({
        material,
        std::move(descriptor_sets)
    });

    m_pending_addition_flag.Set(true, MemoryOrder::RELEASE);
}

void MaterialDescriptorSetManager::AddMaterial(const Handle<Material> &material, FixedArray<Handle<Texture>, max_bound_textures> &&textures)
{
    if (!material.IsValid()) {
        return;
    }

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

        m_material_descriptor_sets.Insert(material, std::move(descriptor_sets));

        return;
    }

    Mutex::Guard guard(m_pending_mutex);

    m_pending_addition.PushBack({
        material,
        std::move(descriptor_sets)
    });

    m_pending_addition_flag.Set(true, MemoryOrder::RELEASE);
}

void MaterialDescriptorSetManager::EnqueueRemoveMaterial(const WeakHandle<Material> &material)
{
    if (!material.IsValid()) {
        return;
    }

    HYP_LOG(Material, LogLevel::DEBUG, "EnqueueRemove material with ID {} from thread {}", material.GetID().Value(), Threads::CurrentThreadID().name);

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
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

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
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    if (!m_pending_addition_flag.Get(MemoryOrder::ACQUIRE)) {
        return;
    }

    Mutex::Guard guard(m_pending_mutex);

    for (auto it = m_pending_removal.Begin(); it != m_pending_removal.End();) {
        const auto material_descriptor_sets_it = m_material_descriptor_sets.FindAs(*it);

        if (material_descriptor_sets_it != m_material_descriptor_sets.End()) {
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

void MaterialDescriptorSetManager::Update(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    const uint descriptor_sets_to_update_flag = m_descriptor_sets_to_update_flag.Get(MemoryOrder::ACQUIRE);

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
