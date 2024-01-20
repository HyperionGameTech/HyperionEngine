#include "Material.hpp"

#include <Engine.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>

#include <util/ByteUtil.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::DescriptorSet;
using renderer::DescriptorKey;
using renderer::ImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::CommandBuffer;
using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(UpdateMaterialRenderData) : renderer::RenderCommand
{
    ID<Material> id;
    MaterialShaderData shader_data;
    SizeType num_bound_textures;
    FixedArray<ID<Texture>, MaterialShaderData::max_bound_textures> bound_texture_ids;

    RENDER_COMMAND(UpdateMaterialRenderData)(
        ID<Material> id,
        const MaterialShaderData &shader_data,
        SizeType num_bound_textures,
        FixedArray<ID<Texture>, MaterialShaderData::max_bound_textures> &&bound_texture_ids
    ) : id(id),
        shader_data(shader_data),
        num_bound_textures(num_bound_textures),
        bound_texture_ids(std::move(bound_texture_ids))
    {
    }
    
    virtual Result operator()()
    {
        shader_data.texture_usage = 0;

        Memory::MemSet(shader_data.texture_index, 0, sizeof(shader_data.texture_index));

        if (num_bound_textures != 0) {
            for (SizeType i = 0; i < bound_texture_ids.Size(); i++) {
                if (bound_texture_ids[i] != Texture::empty_id) {
#if HYP_FEATURES_BINDLESS_TEXTURES
                    shader_data.texture_index[i] = bound_texture_ids[i].ToIndex();
#else
                    shader_data.texture_index[i] = i;
#endif

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
    ID<Material> id;
    SizeType texture_index;
    Texture *texture;
    
    RENDER_COMMAND(UpdateMaterialTexture)(
        ID<Material> id,
        SizeType texture_index,
        Texture *texture
    ) : id(id),
        texture_index(texture_index),
        texture(texture)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const auto descriptor_set_index = DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, id.value - 1, frame_index);

            const auto &descriptor_pool = g_engine->GetGPUInstance()->GetDescriptorPool();
            const DescriptorSetRef descriptor_set = descriptor_pool.GetDescriptorSet(descriptor_set_index);
            auto *descriptor = descriptor_set->GetDescriptor(DescriptorKey::TEXTURES);

            descriptor->SetElementSRV(texture_index, texture->GetImageView());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateMaterialDescriptors) : renderer::RenderCommand
{
    ID<Material> id;
    // map slot index -> image view
    FlatMap<UInt, ImageViewRef> textures;
    renderer::DescriptorSet **descriptor_sets;

    RENDER_COMMAND(CreateMaterialDescriptors)(
        ID<Material> id,
        FlatMap<UInt, ImageViewRef>  &&textures,
        renderer::DescriptorSet **descriptor_sets
    ) : id(id),
        textures(std::move(textures)),
        descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const auto parent_index = DescriptorSet::Index(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES);
            const auto index = DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, id.ToIndex(), frame_index);

            DebugLog(
                LogType::Debug,
                "Adding descriptor set for Material #%u (frame index: %u, descriptor set index: %u)\n",
                id.Value(),
                frame_index,
                UInt(index)
            );

            auto &descriptor_pool = g_engine->GetGPUInstance()->GetDescriptorPool();

            DescriptorSetRef descriptor_set = MakeRenderObject<renderer::DescriptorSet>(
                parent_index,
                UInt(index),
                false
            );
            
            descriptor_pool.AddDescriptorSet(g_engine->GetGPUDevice(), descriptor_set);

            auto *sampler_descriptor = descriptor_set->AddDescriptor<SamplerDescriptor>(DescriptorKey::SAMPLER);
            sampler_descriptor->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());
            
            // set placeholder for all items, then manually set the actual bound items
            auto *image_descriptor = descriptor_set->AddDescriptor<ImageDescriptor>(DescriptorKey::TEXTURES);

            for (UInt texture_index = 0; texture_index < Material::max_textures_to_set; texture_index++) {
                image_descriptor->SetElementSRV(texture_index, &g_engine->GetPlaceholderData().GetImageView2D1x1R8());
            }

            for (const auto &it : textures) {
                const UInt texture_index = it.first;
                const ImageViewRef &image_view = it.second;

                if (texture_index >= Material::max_textures_to_set) {
                    DebugLog(LogType::Error, "Invalid texture bound to material texture slot %u! Index out of range.\n", texture_index);

                    continue;
                }

                if (!image_view.IsValid()) {
                    DebugLog(LogType::Error, "Invalid texture bound to material texture slot %u ImageView is invalid!\n", texture_index);

                    continue;
                }

                image_descriptor->SetElementSRV(texture_index, image_view);
            }

            if (descriptor_pool.IsCreated()) { // creating at runtime, after descriptor sets all created
                result = descriptor_set->Create(
                    g_engine->GetGPUDevice(),
                    &g_engine->GetGPUInstance()->GetDescriptorPool()
                );

                if (result != Result::OK) {
                    UInt previous_index = (frame_index + max_frames_in_flight - 1) % max_frames_in_flight;

                    while (previous_index != frame_index) {
                        if (descriptor_sets[previous_index]) {
                            HYPERION_PASS_ERRORS(descriptor_sets[previous_index]->Destroy(g_engine->GetGPUDevice()), result);

                            descriptor_pool.RemoveDescriptorSet(g_engine->GetGPUDevice(), descriptor_sets[previous_index]);
                            descriptor_sets[previous_index] = nullptr;
                        }

                        previous_index = (previous_index + 1) % max_frames_in_flight;
                    }

                    descriptor_pool.RemoveDescriptorSet(g_engine->GetGPUDevice(), descriptor_set);
                    descriptor_sets[frame_index] = nullptr;

                    return Result::OK;
                }
            }

            descriptor_sets[frame_index] = descriptor_set;
        }

        return result;
    }
};

struct RENDER_COMMAND(DestroyMaterialDescriptors) : renderer::RenderCommand
{
    renderer::DescriptorSet **descriptor_sets;

    RENDER_COMMAND(DestroyMaterialDescriptors)(renderer::DescriptorSet **descriptor_sets)
        : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        auto &descriptor_pool = g_engine->GetGPUInstance()->GetDescriptorPool();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            if (!descriptor_sets[frame_index]) {
                continue;
            }

            // if (descriptor_pool.IsCreated()) { // creating at runtime, after descriptor sets all created
            //     HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Destroy(g_engine->GetGPUDevice()));
            // }

            descriptor_pool.RemoveDescriptorSet(g_engine->GetGPUDevice(), descriptor_sets[frame_index]);
            descriptor_sets[frame_index] = nullptr;
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

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
            renderer::static_mesh_vertex_attributes
        },
        .bucket = Bucket::BUCKET_OPAQUE
      },
      m_is_dynamic(false),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    ResetParameters();
}

Material::Material(Name name, Bucket bucket)
    : BasicObject(name),
      m_render_attributes {
        .shader_definition = ShaderDefinition {
            HYP_NAME(Forward),
            renderer::static_mesh_vertex_attributes
        },
        .bucket = Bucket::BUCKET_OPAQUE
      },
      m_is_dynamic(false),
      m_shader_data_state(ShaderDataState::DIRTY)
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
    m_shader_data_state(ShaderDataState::DIRTY)
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

    if (IsInitCalled()) {
#if !HYP_FEATURES_BINDLESS_TEXTURES
        EnqueueDescriptorSetDestroy();
#endif

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
            InitObject(texture);
        }
    }

#if !HYP_FEATURES_BINDLESS_TEXTURES
    EnqueueDescriptorSetCreate();
#endif

    SetReady(true);

    EnqueueRenderUpdates();
}

void Material::Update()
{
    AssertReady();

    if (!m_shader_data_state.IsDirty()) {
        return;
    }

    EnqueueRenderUpdates();
}

void Material::EnqueueDescriptorSetCreate()
{
    FlatMap<UInt, ImageViewRef> texture_bindings;

    const UInt num_bound_textures = max_textures_to_set;
    
    for (UInt i = 0; i < num_bound_textures; i++) {
        if (const Handle<Texture> &texture = m_textures.ValueAt(i)) {
            if (texture->GetImageView()) {
                texture_bindings[i] = texture->GetImageView();
            }
        }
    }

    PUSH_RENDER_COMMAND(
        CreateMaterialDescriptors,
        m_id,
        std::move(texture_bindings),
        m_descriptor_sets.Data()
    );
}

void Material::EnqueueDescriptorSetDestroy()
{
    PUSH_RENDER_COMMAND(
        DestroyMaterialDescriptors, 
        m_descriptor_sets.Data()
    );
}

void Material::EnqueueRenderUpdates()
{
    AssertReady();
    
    FixedArray<ID<Texture>, MaterialShaderData::max_bound_textures> bound_texture_ids { };

    const UInt num_bound_textures = max_textures_to_set;
    
    for (UInt i = 0; i < num_bound_textures; i++) {
        if (const Handle<Texture> &texture = m_textures.ValueAt(i)) {
            bound_texture_ids[i] = texture->GetID();
        }
    }

    MaterialShaderData shader_data {
        .albedo = GetParameter<Vector4>(MATERIAL_KEY_ALBEDO),
        .packed_params = ShaderVec4<UInt32>(
            ByteUtil::PackColorU32(Vector4(
                GetParameter<Float>(MATERIAL_KEY_ROUGHNESS),
                GetParameter<Float>(MATERIAL_KEY_METALNESS),
                GetParameter<Float>(MATERIAL_KEY_TRANSMISSION),
                GetParameter<Float>(MATERIAL_KEY_NORMAL_MAP_INTENSITY)
            )),
            ByteUtil::PackColorU32(Vector4(
                GetParameter<Float>(MATERIAL_KEY_ALPHA_THRESHOLD)
            )),
            ByteUtil::PackColorU32(Vector4()),
            ByteUtil::PackColorU32(Vector4())
        ),
        .uv_scale = GetParameter<Vector2>(MATERIAL_KEY_UV_SCALE),
        .parallax_height = GetParameter<float>(MATERIAL_KEY_PARALLAX_HEIGHT)
    };

    PUSH_RENDER_COMMAND(
        UpdateMaterialRenderData,
        m_id,
        shader_data,
        num_bound_textures,
        std::move(bound_texture_ids)
    );

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Material::EnqueueTextureUpdate(TextureKey key)
{
    const SizeType texture_index = decltype(m_textures)::EnumToOrdinal(key);

    Texture *texture = m_textures.Get(key).Get();
    AssertThrow(texture != nullptr);

    PUSH_RENDER_COMMAND(UpdateMaterialTexture, 
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

    m_render_attributes.shader_definition = shader.IsValid()
        ? shader->GetCompiledShader().GetDefinition()
        : ShaderDefinition { };

    m_shader = std::move(shader);
    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Material::SetParameter(MaterialKey key, const Parameter &value)
{
    if (m_parameters[key] == value) {
        return;
    }

    m_parameters.Set(key, value);
    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Material::ResetParameters()
{
    m_parameters = DefaultParameters();

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Material::SetTexture(TextureKey key, Handle<Texture> &&texture)
{
    if (m_textures[key] == texture) {
        return;
    }

    if (IsInitCalled()) {
        // release current texture
        if (m_textures[key] != nullptr) {
            m_textures[key].Reset();
        }

        InitObject(texture);
    }

    m_textures.Set(key, std::move(texture));

#if !HYP_FEATURES_BINDLESS_TEXTURES
    if (texture && IsReady()) {
        EnqueueTextureUpdate(key);
    }
#endif

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Material::SetTexture(TextureKey key, const Handle<Texture> &texture)
{
    SetTexture(key, Handle<Texture>(texture));
}

void Material::SetTextureAtIndex(UInt index, const Handle<Texture> &texture)
{
    const TextureKey key = static_cast<TextureKey>(m_textures.OrdinalToEnum(index));

    return SetTexture(key, texture);
}

const Handle<Texture> &Material::GetTexture(TextureKey key) const
{
    return m_textures.Get(key);
}

const Handle<Texture> &Material::GetTextureAtIndex(UInt index) const
{
    const TextureKey key = static_cast<TextureKey>(m_textures.OrdinalToEnum(index));

    return GetTexture(key);
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

MaterialGroup::MaterialGroup()
    : BasicObject()
{
}

MaterialGroup::~MaterialGroup()
{
    Teardown();
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

    OnTeardown([this](...) {
        m_materials.Clear();
    });
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

void MaterialCache::Add(const Handle<Material> &material)
{
    if (!material) {
        return;
    }

    std::lock_guard guard(m_mutex);

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

Handle<Material> MaterialCache::GetOrCreate(
    MaterialAttributes attributes,
    const Material::ParameterTable &parameters,
    const Material::TextureSet &textures
)
{
    if (!attributes.shader_definition) {
        attributes.shader_definition = ShaderDefinition {
            HYP_NAME(Forward),
            renderer::static_mesh_vertex_attributes
        };
    }

    HashCode hc;
    hc.Add(attributes.GetHashCode());
    hc.Add(parameters.GetHashCode());
    hc.Add(textures.GetHashCode());

    std::lock_guard guard(m_mutex);

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

static struct MaterialScriptBindings : ScriptBindingsBase
{
    MaterialScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<Material>())
    {
    }

    virtual void Generate(APIInstance &api_instance) override
    {
        api_instance.Module(Config::global_module_name)
            .Class<Handle<Material>>(
                "Material",
                {
                    API::NativeMemberDefine(
                        "$construct",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxFn< Handle<Material>, void *, ScriptCreateObject<Material> >
                    ),
                    API::NativeMemberDefine(
                        "GetID",
                        BuiltinTypes::UNSIGNED_INT,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxFn< UInt32, const Handle<Material> &, ScriptGetHandleIDValue<Material> >
                    ),
                    API::NativeMemberDefine(
                        "Init",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFnWrapped< void, Handle<Material>, Material, &Material::Init >
                    ),
                    API::NativeMemberDefine(
                        "SetTextureAtIndex",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "index", BuiltinTypes::UNSIGNED_INT },
                            { "value", BuiltinTypes::ANY }
                        },
                        CxxMemberFnWrapped< void, Handle<Material>, Material, UInt, const Handle<Texture> &, &Material::SetTextureAtIndex >
                    ),
                    API::NativeMemberDefine(
                        "GetTextureAtIndex",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "index", BuiltinTypes::UNSIGNED_INT }
                        },
                        CxxMemberFnWrapped< const Handle<Texture> &, Handle<Material>, Material, UInt, &Material::GetTextureAtIndex >
                    )
                }
            );
    }
} material_script_bindings = { };

} // namespace hyperion::v2
