#include "Material.hpp"

#include <Engine.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <util/ByteUtil.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::DescriptorSet;
using renderer::DescriptorKey;
using renderer::ImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::CommandBuffer;
using renderer::Result;

struct RENDER_COMMAND(UpdateMaterialRenderData) : RenderCommandBase2
{
    Material::ID id;
    MaterialShaderData shader_data;
    SizeType num_bound_textures;
    FixedArray<Texture::ID, MaterialShaderData::max_bound_textures> bound_texture_ids;

    RENDER_COMMAND(UpdateMaterialRenderData)(
        Material::ID id,
        const MaterialShaderData &shader_data,
        SizeType num_bound_textures,
        FixedArray<Texture::ID, MaterialShaderData::max_bound_textures> &&bound_texture_ids
    ) : id(id),
        shader_data(shader_data),
        num_bound_textures(num_bound_textures),
        bound_texture_ids(std::move(bound_texture_ids))
    {
    }
    
    virtual Result operator()(Engine *engine)
    {
        shader_data.texture_usage = 0;

        Memory::Set(shader_data.texture_index, 0, sizeof(shader_data.texture_index));

        if (num_bound_textures != 0) {
            for (SizeType i = 0; i < bound_texture_ids.Size(); i++) {
                if (bound_texture_ids[i] != Texture::empty_id) {
#if HYP_FEATURES_BINDLESS_TEXTURES
                    shader_data.texture_index[i] = ids[i].ToIndex();
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

        Engine::Get()->GetRenderData()->materials.Set(id.ToIndex(), shader_data);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateMaterialTexture) : RenderCommandBase2
{
    Material::ID id;
    SizeType texture_index;
    Texture *texture;
    
    RENDER_COMMAND(UpdateMaterialTexture)(
        Material::ID id,
        SizeType texture_index,
        Texture *texture
    ) : id(id),
        texture_index(texture_index),
        texture(texture)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const auto descriptor_set_index = DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, id.value - 1, frame_index);

            const auto &descriptor_pool = Engine::Get()->GetInstance()->GetDescriptorPool();
            const auto *descriptor_set = descriptor_pool.GetDescriptorSet(descriptor_set_index);
            auto *descriptor = descriptor_set->GetDescriptor(DescriptorKey::TEXTURES);

            descriptor->SetSubDescriptor({
                .element_index = static_cast<UInt>(texture_index),
                .image_view = &texture->GetImageView()
            });
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateMaterialDescriptors) : RenderCommandBase2
{
    Material::ID id;
    Handle<Texture> *textures;
    renderer::DescriptorSet **descriptor_sets;

    RENDER_COMMAND(CreateMaterialDescriptors)(
        Material::ID id,
        Handle<Texture> *textures,
        renderer::DescriptorSet **descriptor_sets
    ) : id(id),
        textures(textures),
        descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const auto parent_index = DescriptorSet::Index(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES);
            const auto index = DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, id.ToIndex(), frame_index);

            auto &descriptor_pool = Engine::Get()->GetInstance()->GetDescriptorPool();

            auto *descriptor_set = descriptor_pool.AddDescriptorSet(
                Engine::Get()->GetDevice(),
                std::make_unique<DescriptorSet>(
                    parent_index,
                    static_cast<UInt>(index),
                    false
                )
            );

            auto *sampler_descriptor = descriptor_set->AddDescriptor<SamplerDescriptor>(DescriptorKey::SAMPLER);

            sampler_descriptor->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerLinear() // TODO: get proper sampler based on req's of image
            });
            
            auto *image_descriptor = descriptor_set->AddDescriptor<ImageDescriptor>(DescriptorKey::TEXTURES);

            for (UInt texture_index = 0; texture_index < Material::max_textures_to_set; texture_index++) {
                if (auto &texture = textures[texture_index]) {
                    image_descriptor->SetSubDescriptor({
                        .element_index = texture_index,
                        .image_view = &texture->GetImageView()
                    });
                } else {
                    image_descriptor->SetSubDescriptor({
                        .element_index = texture_index,
                        .image_view = &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8()
                    });
                }
            }

            if (descriptor_pool.IsCreated()) { // creating at runtime, after descriptor sets all created
                HYPERION_BUBBLE_ERRORS(descriptor_set->Create(
                    Engine::Get()->GetDevice(),
                    &Engine::Get()->GetInstance()->GetDescriptorPool()
                ));
            }

            descriptor_sets[frame_index] = descriptor_set;
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyMaterialDescriptors) : RenderCommandBase2
{
    renderer::DescriptorSet **descriptor_sets;

    RENDER_COMMAND(DestroyMaterialDescriptors)(renderer::DescriptorSet **descriptor_sets)
        : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        auto &descriptor_pool = Engine::Get()->GetInstance()->GetDescriptorPool();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            if (!descriptor_sets[frame_index]) {
                continue;
            }

            if (descriptor_pool.IsCreated()) { // creating at runtime, after descriptor sets all created
                HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Destroy(Engine::Get()->GetDevice()));
            }

            descriptor_pool.RemoveDescriptorSet(descriptor_sets[frame_index]);
            descriptor_sets[frame_index] = nullptr;
        }

        HYPERION_RETURN_OK;
    }
};

Material::Material()
    : EngineComponentBase(),
      m_render_attributes { .bucket = Bucket::BUCKET_OPAQUE },
      m_shader_data_state(ShaderDataState::DIRTY)
{
    ResetParameters();
}

Material::Material(const String &name, Bucket bucket)
    : EngineComponentBase(name),
      m_render_attributes { .bucket = bucket },
      m_shader_data_state(ShaderDataState::DIRTY)
{
    ResetParameters();
}

Material::~Material()
{
    Teardown();
}

void Material::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(Engine::Get());

    for (SizeType i = 0; i < m_textures.Size(); i++) {
        if (auto &texture = m_textures.ValueAt(i)) {
            Engine::Get()->InitObject(texture);
        }
    }

#if !HYP_FEATURES_BINDLESS_TEXTURES
    EnqueueDescriptorSetCreate();
#endif

    SetReady(true);

    EnqueueRenderUpdates();

    OnTeardown([this]() {
        SetReady(false);

        for (SizeType i = 0; i < m_textures.Size(); i++) {
            if (auto &texture = m_textures.ValueAt(i)) {
                Engine::Get()->SafeReleaseHandle<Texture>(std::move(texture));
            }
        }

#if !HYP_FEATURES_BINDLESS_TEXTURES
        EnqueueDescriptorSetDestroy();
#endif

        HYP_FLUSH_RENDER_QUEUE();
    });
}

void Material::Update(Engine *engine)
{
    AssertReady();

    if (!m_shader_data_state.IsDirty()) {
        return;
    }

    EnqueueRenderUpdates();
}

void Material::EnqueueDescriptorSetCreate()
{
    RenderCommands::Push<RENDER_COMMAND(CreateMaterialDescriptors)>(
        m_id,
        m_textures.Data(),
        m_descriptor_sets.Data()
    );
}

void Material::EnqueueDescriptorSetDestroy()
{
    RenderCommands::Push<RENDER_COMMAND(DestroyMaterialDescriptors)>(
        m_descriptor_sets.Data()
    );
}

void Material::EnqueueRenderUpdates()
{
    AssertReady();
    
    FixedArray<Texture::ID, MaterialShaderData::max_bound_textures> bound_texture_ids { };

    const UInt num_bound_textures = max_textures_to_set;
    
    for (UInt i = 0; i < num_bound_textures; i++) {
        if (const auto &texture = m_textures.ValueAt(i)) {
            bound_texture_ids[i] = texture->GetID();
        }
    }

    MaterialShaderData shader_data {
        .albedo = GetParameter<Vector4>(MATERIAL_KEY_ALBEDO),
        .packed_params = ShaderVec4<UInt32>(
            ByteUtil::PackColorU32(Vector4(
                GetParameter<float>(MATERIAL_KEY_ROUGHNESS),
                GetParameter<float>(MATERIAL_KEY_METALNESS),
                GetParameter<float>(MATERIAL_KEY_TRANSMISSION),
                0.0f
            )),
            0, 0,
            ByteUtil::PackColorU32(Vector4(
                0.0f,
                0.0f,
                0.0f,
                0.0f
            ))
        ),
        .uv_scale = GetParameter<Vector2>(MATERIAL_KEY_UV_SCALE),
        .parallax_height = GetParameter<float>(MATERIAL_KEY_PARALLAX_HEIGHT)
    };

    RenderCommands::Push<RENDER_COMMAND(UpdateMaterialRenderData)>(
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

    RenderCommands::Push<RENDER_COMMAND(UpdateMaterialTexture)>(
        m_id,
        texture_index,
        texture
    );
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
    m_parameters.Set(MATERIAL_KEY_ALBEDO,          Vector4(1.0f));
    m_parameters.Set(MATERIAL_KEY_METALNESS,       0.0f);
    m_parameters.Set(MATERIAL_KEY_ROUGHNESS,       0.65f);
    m_parameters.Set(MATERIAL_KEY_TRANSMISSION,    0.0f);
    m_parameters.Set(MATERIAL_KEY_EMISSIVE,        0.0f);
    m_parameters.Set(MATERIAL_KEY_SPECULAR,        0.0f);
    m_parameters.Set(MATERIAL_KEY_SPECULAR_TINT,   0.0f);
    m_parameters.Set(MATERIAL_KEY_ANISOTROPIC,     0.0f);
    m_parameters.Set(MATERIAL_KEY_SHEEN,           0.0f);
    m_parameters.Set(MATERIAL_KEY_SHEEN_TINT,      0.0f);
    m_parameters.Set(MATERIAL_KEY_CLEARCOAT,       0.0f);
    m_parameters.Set(MATERIAL_KEY_CLEARCOAT_GLOSS, 0.0f);
    m_parameters.Set(MATERIAL_KEY_SUBSURFACE,      0.0f);
    m_parameters.Set(MATERIAL_KEY_UV_SCALE,        Vector2(1.0f));
    m_parameters.Set(MATERIAL_KEY_PARALLAX_HEIGHT, 0.005f);

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
            Engine::Get()->SafeReleaseHandle<Texture>(std::move(m_textures[key]));
        }

        Engine::Get()->InitObject(texture);
    }

    m_textures.Set(key, std::move(texture));

#if !HYP_FEATURES_BINDLESS_TEXTURES
    if (texture && IsReady()) {
        EnqueueTextureUpdate(key);
    }
#endif

    m_shader_data_state |= ShaderDataState::DIRTY;
}

Texture *Material::GetTexture(TextureKey key)
{
    return m_textures.Get(key).Get();
}

const Texture *Material::GetTexture(TextureKey key) const
{
    return m_textures.Get(key).Get();
}

MaterialGroup::MaterialGroup()
    : EngineComponentBase()
{
}

MaterialGroup::~MaterialGroup()
{
    Teardown();
}

void MaterialGroup::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(Engine::Get());

    for (auto &it : m_materials) {
        Engine::Get()->InitObject(it.second);
    }

    OnTeardown([this](...) {
        m_materials.clear();
    });
}

void MaterialGroup::Add(const std::string &name, Handle<Material> &&material)
{
    if (IsInitCalled()) {
        Engine::Get()->InitObject(material);
    }

    m_materials[name] = std::move(material);
}

bool MaterialGroup::Remove(const std::string &name)
{
    const auto it = m_materials.find(name);

    if (it != m_materials.end()) {
        m_materials.erase(it);

        return true;
    }

    return false;
}

} // namespace hyperion::v2
