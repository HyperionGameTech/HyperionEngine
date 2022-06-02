#include "material.h"

#include <engine.h>

#include <rendering/backend/renderer_descriptor_set.h>

#include <types.h>

namespace hyperion::v2 {

using renderer::DescriptorSet;
using renderer::SamplerDescriptor;
using renderer::CommandBuffer;

Material::Material(const char *tag)
    : EngineComponentBase(),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    size_t len = std::strlen(tag);
    m_tag = new char[len + 1];
    std::strcpy(m_tag, tag);

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

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_MATERIALS, [this](Engine *engine) {
        for (size_t i = 0; i < m_textures.Size(); i++) {
            if (auto &texture = m_textures.ValueAt(i)) {
                if (texture == nullptr) {
                    continue;
                }

                texture.Init();
            }
        }

        SetReady(true);

#if !HYP_FEATURES_BINDLESS_TEXTURES
        // add a descriptor set w/ each texture
        engine->render_scheduler.Enqueue([this, engine](...) {
            for (uint frame_index = 0; frame_index < 2; frame_index++) {
                const auto parent_index = DescriptorSet::Index(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES);
                const auto index        = DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, m_id.value - 1, frame_index);

                auto &descriptor_pool = engine->GetInstance()->GetDescriptorPool();

                auto *descriptor_set = descriptor_pool.AddDescriptorSet(std::make_unique<DescriptorSet>(
                    parent_index,
                    uint(index),
                    false
                ));

                // now add each possible texture (max is 16 because of device limitations)
                // TODO: only add used texture slots, and fixup indices
                
                auto *descriptor = descriptor_set->AddDescriptor<SamplerDescriptor>(0);

                uint texture_index = 0;

                for (size_t i = 0; i < m_textures.Size(); i++) {
                    if (auto &texture = m_textures.ValueAt(i)) {
                        if (texture == nullptr) {
                            ++texture_index;

                            continue;
                        }

                        descriptor->AddSubDescriptor({
                            .element_index = texture_index,
                            .image_view    = &texture->GetImageView(),
                            .sampler       = &texture->GetSampler()
                        });

                        ++texture_index;

                        if (texture_index == DescriptorSet::max_material_texture_samplers) {
                            break;
                        }
                    }
                }

                if (descriptor_pool.IsCreated()) { // creating at runtime, after descriptor sets all created
                    HYPERION_BUBBLE_ERRORS(descriptor_pool.CreateDescriptorSet(engine->GetDevice(), uint(index)));
                }
            }

            HYPERION_RETURN_OK;
        });
#endif
        //EnqueueRenderUpdates(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_MATERIALS, [this](Engine *engine) {
            m_textures.Clear();
            
            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }), engine);
    }));
}

void Material::Update(Engine *engine)
{
    AssertReady();

    if (!m_shader_data_state.IsDirty()) {
        return;
    }

    EnqueueRenderUpdates(engine);
}

void Material::EnqueueRenderUpdates(Engine *engine)
{
    AssertReady();
    
    std::array<Texture::ID, MaterialShaderData::max_bound_textures> bound_texture_ids{};

    const size_t num_bound_textures = MathUtil::Min(
        m_textures.Size(),
        MaterialShaderData::max_bound_textures
    );
    
    for (size_t i = 0; i < num_bound_textures; i++) {
        if (const auto &texture = m_textures.ValueAt(i)) {
            bound_texture_ids[i] = texture->GetId();
        }
    }

    engine->render_scheduler.Enqueue([this, engine, bound_texture_ids](...) {
        MaterialShaderData shader_data{
            .albedo          = GetParameter<Vector4>(MATERIAL_KEY_ALBEDO),
            .metalness       = GetParameter<float>(MATERIAL_KEY_METALNESS),
            .roughness       = GetParameter<float>(MATERIAL_KEY_ROUGHNESS),
            .subsurface      = GetParameter<float>(MATERIAL_KEY_SUBSURFACE),
            .specular        = GetParameter<float>(MATERIAL_KEY_SPECULAR),
            .specular_tint   = GetParameter<float>(MATERIAL_KEY_SPECULAR_TINT),
            .anisotropic     = GetParameter<float>(MATERIAL_KEY_ANISOTROPIC),
            .sheen           = GetParameter<float>(MATERIAL_KEY_SHEEN),
            .sheen_tint      = GetParameter<float>(MATERIAL_KEY_SHEEN_TINT),
            .clearcoat       = GetParameter<float>(MATERIAL_KEY_CLEARCOAT),
            .clearcoat_gloss = GetParameter<float>(MATERIAL_KEY_CLEARCOAT_GLOSS),
            .emissiveness    = GetParameter<float>(MATERIAL_KEY_EMISSIVENESS),
            .uv_scale        = GetParameter<float>(MATERIAL_KEY_UV_SCALE),
            .parallax_height = GetParameter<float>(MATERIAL_KEY_PARALLAX_HEIGHT)
        };

        shader_data.texture_usage = 0;

        for (size_t i = 0; i < bound_texture_ids.size(); i++) {
            if (bound_texture_ids[i] != Texture::empty_id) {
#if HYP_FEATURES_BINDLESS_TEXTURES
                shader_data.texture_index[i] = bound_texture_ids[i].value - 1;
#else
                shader_data.texture_index[i] = i;
#endif

                shader_data.texture_usage |= 1 << i;
            }
        }

        engine->shader_globals->materials.Set(m_id.value - 1, shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Material::EnqueueTextureUpdate(TextureKey key)
{
    // TODO: (thread safety) - cannot copy `texture` w/ IncRef() because it is not
    // CopyConstructable. So we have to just fetch it from m_textures (in the render thread).
    // we should try to refactor this to not use std::function at all.
    GetEngine()->render_scheduler.Enqueue([this, key](CommandBuffer *command_buffer, uint frame_index) {
        auto &texture            = m_textures.Get(key);
        const auto texture_index = decltype(m_textures)::EnumToOrdinal(key);

        // update descriptor set for the given frame_index
        // these scheduled tasks are executed before descriptors sets are updated,
        // so it won't be updating a descriptor set that is in use
        auto *engine                    = GetEngine();
        const auto descriptor_set_index = DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, m_id.value - 1, frame_index);
        
        auto &descriptor_pool = engine->GetInstance()->GetDescriptorPool();
        auto *descriptor_set  = descriptor_pool.GetDescriptorSet(descriptor_set_index);
        auto *descriptor      = descriptor_set->GetDescriptor(0);

        descriptor->AddSubDescriptor({
            .element_index = static_cast<uint>(texture_index),
            .image_view    = &texture->GetImageView(),
            .sampler       = &texture->GetSampler()
        });

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
    m_parameters.Set(MATERIAL_KEY_ROUGHNESS,       0.5f);
    m_parameters.Set(MATERIAL_KEY_SUBSURFACE,      0.0f);
    m_parameters.Set(MATERIAL_KEY_SPECULAR,        0.0f);
    m_parameters.Set(MATERIAL_KEY_SPECULAR_TINT,   0.0f);
    m_parameters.Set(MATERIAL_KEY_ANISOTROPIC,     0.0f);
    m_parameters.Set(MATERIAL_KEY_SHEEN,           0.0f);
    m_parameters.Set(MATERIAL_KEY_SHEEN_TINT,      0.0f);
    m_parameters.Set(MATERIAL_KEY_CLEARCOAT,       0.0f);
    m_parameters.Set(MATERIAL_KEY_CLEARCOAT_GLOSS, 0.0f);
    m_parameters.Set(MATERIAL_KEY_EMISSIVENESS,    0.0f);
    m_parameters.Set(MATERIAL_KEY_UV_SCALE,        Vector2(1.0f));
    m_parameters.Set(MATERIAL_KEY_PARALLAX_HEIGHT, 0.08f);
}

void Material::SetTexture(TextureKey key, Ref<Texture> &&texture)
{
    if (m_textures[key] == texture) {
        return;
    }

    if (texture && IsReady()) {
        texture.Init();

        m_textures.Set(key, std::forward<Ref<Texture>>(texture));

#if !HYP_FEATURES_BINDLESS_TEXTURES
        EnqueueTextureUpdate(key);
#endif
    } else {
        m_textures.Set(key, std::forward<Ref<Texture>>(texture));
    }


    m_shader_data_state |= ShaderDataState::DIRTY;
}

Texture *Material::GetTexture(TextureKey key) const
{
    return m_textures.Get(key).ptr;
}

MaterialGroup::MaterialGroup() = default;
MaterialGroup::~MaterialGroup() = default;

} // namespace hyperion::v2
