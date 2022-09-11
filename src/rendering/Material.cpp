#include "Material.hpp"

#include <Engine.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

using renderer::DescriptorSet;
using renderer::DescriptorKey;
using renderer::ImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::CommandBuffer;

Material::Material(const String &name)
    : EngineComponentBase(),
      m_name(name),
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

    EngineComponentBase::Init(engine);

    for (SizeType i = 0; i < m_textures.Size(); i++) {
        if (auto &texture = m_textures.ValueAt(i)) {
            engine->InitObject(texture);
        }
    }

#if !HYP_FEATURES_BINDLESS_TEXTURES
    EnqueueDescriptorSetCreate();
#endif

    SetReady(true);

    EnqueueRenderUpdates();

    OnTeardown([this]() {
        SetReady(false);

        auto *engine = GetEngine();

        for (SizeType i = 0; i < m_textures.Size(); i++) {
            if (auto &texture = m_textures.ValueAt(i)) {
                engine->SafeReleaseRenderResource<Texture>(std::move(texture));
            }
        }

#if !HYP_FEATURES_BINDLESS_TEXTURES
        EnqueueDescriptorSetDestroy();
#endif

        HYP_FLUSH_RENDER_QUEUE(engine);
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
    // add a descriptor set w/ each texture
    GetEngine()->GetRenderScheduler().Enqueue([this](...) {
        const auto *engine = GetEngine();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const auto parent_index = DescriptorSet::Index(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES);
            const auto index = DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, m_id.value - 1, frame_index);

            auto &descriptor_pool = engine->GetInstance()->GetDescriptorPool();

            auto *descriptor_set = descriptor_pool.AddDescriptorSet(
                engine->GetDevice(),
                std::make_unique<DescriptorSet>(
                    parent_index,
                    static_cast<UInt>(index),
                    false
                )
            );

            auto *sampler_descriptor = descriptor_set->AddDescriptor<SamplerDescriptor>(DescriptorKey::SAMPLER);

            sampler_descriptor->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &engine->GetPlaceholderData().GetSamplerLinear() // TODO: get proper sampler based on req's of image
            });
            
            auto *image_descriptor = descriptor_set->AddDescriptor<ImageDescriptor>(DescriptorKey::TEXTURES);

            for (UInt texture_index = 0; texture_index < max_textures_to_set; texture_index++) {
                if (auto &texture = m_textures.ValueAt(texture_index)) {
                    image_descriptor->SetSubDescriptor({
                        .element_index = texture_index,
                        .image_view = &texture->GetImageView()
                    });
                } else {
                    image_descriptor->SetSubDescriptor({
                        .element_index = texture_index,
                        .image_view = &engine->GetPlaceholderData().GetImageView2D1x1R8()
                    });
                }
            }

            if (descriptor_pool.IsCreated()) { // creating at runtime, after descriptor sets all created
                HYPERION_BUBBLE_ERRORS(descriptor_set->Create(
                    engine->GetDevice(),
                    &engine->GetInstance()->GetDescriptorPool()
                ));
            }

            m_descriptor_sets[frame_index] = descriptor_set;
        }

        HYPERION_RETURN_OK;
    });
}

void Material::EnqueueDescriptorSetDestroy()
{
    GetEngine()->GetRenderScheduler().Enqueue([this](...) {
        const auto *engine = GetEngine();
        auto &descriptor_pool = engine->GetInstance()->GetDescriptorPool();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            if (!m_descriptor_sets[frame_index]) {
                continue;
            }

            DebugLog(LogType::Debug, "Destroy descriptor set   %u   %u\n", m_descriptor_sets[frame_index]->GetRealIndex(), frame_index);
            // HYP_BREAKPOINT;
            descriptor_pool.RemoveDescriptorSet(m_descriptor_sets[frame_index]);
        }
        
        m_descriptor_sets = { };

        HYPERION_RETURN_OK;
    });
}

void Material::EnqueueRenderUpdates()
{
    AssertReady();
    
    FixedArray<Texture::ID, MaterialShaderData::max_bound_textures> bound_texture_ids { };

    const SizeType num_bound_textures = max_textures_to_set;
    
    for (SizeType i = 0; i < num_bound_textures; i++) {
        if (const auto &texture = m_textures.ValueAt(i)) {
            bound_texture_ids[i] = texture->GetID();
        }
    }

    GetEngine()->GetRenderScheduler().Enqueue([this, ids = std::move(bound_texture_ids)](...) mutable {
        MaterialShaderData shader_data {
            .albedo = GetParameter<Vector4>(MATERIAL_KEY_ALBEDO),
            .metalness = GetParameter<float>(MATERIAL_KEY_METALNESS),
            .roughness = GetParameter<float>(MATERIAL_KEY_ROUGHNESS),
            .uv_scale = GetParameter<float>(MATERIAL_KEY_UV_SCALE),
            .parallax_height = GetParameter<float>(MATERIAL_KEY_PARALLAX_HEIGHT)
        };

        shader_data.texture_usage = 0;

        if (num_bound_textures != 0) {
            for (SizeType i = 0; i < ids.Size(); i++) {
                if (ids[i] != Texture::empty_id) {
#if HYP_FEATURES_BINDLESS_TEXTURES
                    shader_data.texture_index[i] = ids[i].value - 1;
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

        GetEngine()->GetShaderData()->materials.Set(m_id.value - 1, shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Material::EnqueueTextureUpdate(TextureKey key)
{
    GetEngine()->GetRenderScheduler().Enqueue([this, key, texture = m_textures.Get(key)](...) {
        const auto texture_index = decltype(m_textures)::EnumToOrdinal(key);

        // update descriptor set for the given frame_index
        // these scheduled tasks are executed before descriptors sets are updated,
        // so it won't be updating a descriptor set that is in use
        const auto *engine = GetEngine();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const auto descriptor_set_index = DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, m_id.value - 1, frame_index);

            const auto &descriptor_pool = engine->GetInstance()->GetDescriptorPool();
            const auto *descriptor_set = descriptor_pool.GetDescriptorSet(descriptor_set_index);
            auto *descriptor = descriptor_set->GetDescriptor(DescriptorKey::TEXTURES);

            descriptor->SetSubDescriptor({
                .element_index = static_cast<UInt>(texture_index),
                .image_view = &texture->GetImageView()
            });
        }

        HYPERION_RETURN_OK;
    });
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
    m_parameters.Set(MATERIAL_KEY_SUBSURFACE,      0.0f);
    m_parameters.Set(MATERIAL_KEY_SPECULAR,        0.0f);
    m_parameters.Set(MATERIAL_KEY_SPECULAR_TINT,   0.0f);
    m_parameters.Set(MATERIAL_KEY_ANISOTROPIC,     0.0f);
    m_parameters.Set(MATERIAL_KEY_SHEEN,           0.0f);
    m_parameters.Set(MATERIAL_KEY_SHEEN_TINT,      0.0f);
    m_parameters.Set(MATERIAL_KEY_CLEARCOAT,       0.0f);
    m_parameters.Set(MATERIAL_KEY_CLEARCOAT_GLOSS, 0.0f);
    m_parameters.Set(MATERIAL_KEY_EMISSIVENESS,    0.0f);
    m_parameters.Set(MATERIAL_KEY_UV_SCALE,        1.0f);
    m_parameters.Set(MATERIAL_KEY_PARALLAX_HEIGHT, 0.005f);

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Material::SetTexture(TextureKey key, Handle<Texture> &&texture)
{
    if (m_textures[key] == texture) {
        return;
    }

    if (m_textures[key] != nullptr) {
        GetEngine()->SafeReleaseRenderResource<Texture>(std::move(m_textures[key]));
    }

    if (texture && IsInitCalled()) {
        GetEngine()->InitObject(texture);
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

MaterialGroup::MaterialGroup() = default;
MaterialGroup::~MaterialGroup() = default;

} // namespace hyperion::v2
