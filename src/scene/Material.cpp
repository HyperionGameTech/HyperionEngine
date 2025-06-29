/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Material.hpp>
#include <scene/Texture.hpp>

#include <rendering/RenderMaterial.hpp>

#include <rendering/backend/RenderConfig.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion {

#pragma region Material

const Material::ParameterTable& Material::DefaultParameters()
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
          .shader_definition = ShaderDefinition { NAME("Forward"), static_mesh_vertex_attributes },
          .bucket = RB_OPAQUE,
          .fill_mode = FM_FILL,
          .blend_function = BlendFunction::None(),
          .cull_faces = FCM_BACK,
          .flags = MaterialAttributeFlags::DEPTH_WRITE | MaterialAttributeFlags::DEPTH_TEST
      },
      m_is_dynamic(false),
      m_mutation_state(DataMutationState::CLEAN),
      m_render_resource(nullptr)
{
    ResetParameters();
}

Material::Material(Name name, RenderBucket rb)
    : m_name(name),
      m_render_attributes {
          .shader_definition = ShaderDefinition { NAME("Forward"), static_mesh_vertex_attributes },
          .bucket = rb
      },
      m_is_dynamic(false),
      m_mutation_state(DataMutationState::CLEAN),
      m_render_resource(nullptr)
{
    ResetParameters();
}

Material::Material(Name name, const MaterialAttributes& attributes)
    : Material(name, attributes, DefaultParameters(), TextureSet {})
{
}

Material::Material(Name name, const MaterialAttributes& attributes, const ParameterTable& parameters, const TextureSet& textures)
    : m_name(name),
      m_parameters(parameters),
      m_textures(textures),
      m_render_attributes(attributes),
      m_is_dynamic(false),
      m_mutation_state(DataMutationState::CLEAN),
      m_render_resource(nullptr)
{
}

Material::~Material()
{
    if (m_render_resource != nullptr)
    {
        FreeResource(m_render_resource);
    }

    SetReady(false);

    for (SizeType i = 0; i < m_textures.Size(); i++)
    {
        m_textures.ValueAt(i).Reset();
    }
}

void Material::Init()
{
    m_render_resource = AllocateResource<RenderMaterial>(this);

    FlatMap<MaterialTextureKey, Handle<Texture>> textures;

    for (SizeType i = 0; i < m_textures.Size(); i++)
    {
        MaterialTextureKey key = m_textures.KeyAt(i);

        if (Handle<Texture>& texture = m_textures.ValueAt(i))
        {
            InitObject(texture);

            textures.Set(key, texture);
        }
    }

    m_render_resource->SetTextures(std::move(textures));

    m_mutation_state |= DataMutationState::DIRTY;

    SetReady(true);

    EnqueueRenderUpdates();
}

void Material::EnqueueRenderUpdates()
{
    AssertReady();

    static const bool is_bindless_supported = g_render_backend->GetRenderConfig().IsBindlessSupported();

    if (!m_mutation_state.IsDirty())
    {
        HYP_LOG_ONCE(Material, Warning, "EnqueueRenderUpdates called on material with Id {} (name: {}) that is not dirty", GetID(), GetName());

        return;
    }

    static const uint32 num_bound_textures = MathUtil::Min(
        max_textures,
        is_bindless_supported ? max_bindless_resources : max_bound_textures);

    Array<Id<Texture>> bound_texture_ids;
    bound_texture_ids.Resize(num_bound_textures);

    for (uint32 i = 0; i < num_bound_textures; i++)
    {
        if (const Handle<Texture>& texture = m_textures.ValueAt(i))
        {
            bound_texture_ids[i] = texture->GetID();
        }
    }

    m_render_resource->SetBoundTextureIDs(bound_texture_ids);

    MaterialShaderData buffer_data {
        .albedo = GetParameter<Vec4f>(MATERIAL_KEY_ALBEDO),
        .packed_params = Vec4u(
            ByteUtil::PackVec4f(Vec4f(
                GetParameter<float>(MATERIAL_KEY_ROUGHNESS),
                GetParameter<float>(MATERIAL_KEY_METALNESS),
                GetParameter<float>(MATERIAL_KEY_TRANSMISSION),
                GetParameter<float>(MATERIAL_KEY_NORMAL_MAP_INTENSITY))),
            ByteUtil::PackVec4f(Vec4f(
                GetParameter<float>(MATERIAL_KEY_ALPHA_THRESHOLD))),
            ByteUtil::PackVec4f(Vec4f {}),
            ByteUtil::PackVec4f(Vec4f {})),
        .uv_scale = GetParameter<Vec2f>(MATERIAL_KEY_UV_SCALE),
        .parallax_height = GetParameter<float>(MATERIAL_KEY_PARALLAX_HEIGHT)
    };

    m_render_resource->SetBufferData(buffer_data);

    m_mutation_state = DataMutationState::CLEAN;
}

void Material::SetParameter(MaterialKey key, const Parameter& value)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting parameter on static material with Id {} (name: {})", GetID(), GetName());
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    if (m_parameters[key] == value)
    {
        return;
    }

    m_parameters.Set(key, value);

    if (IsInitCalled())
    {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::SetParameters(const ParameterTable& parameters)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting parameters on static material with Id {} (name: {})", GetID(), GetName());
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    m_parameters = parameters;

    if (IsInitCalled())
    {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::ResetParameters()
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Resetting parameters on static material with Id {} (name: {})", GetID(), GetName());
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    m_parameters = DefaultParameters();

    if (IsInitCalled())
    {
        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::SetTexture(MaterialTextureKey key, const Handle<Texture>& texture)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting texture on static material with Id {} (name: {})", GetID(), GetName());
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    if (m_textures[key] == texture)
    {
        return;
    }

    m_textures.Set(key, texture);

    if (IsInitCalled())
    {
        InitObject(texture);

        m_render_resource->SetTexture(key, texture);

        m_mutation_state |= DataMutationState::DIRTY;
    }
}

void Material::SetTextureAtIndex(uint32 index, const Handle<Texture>& texture)
{
    return SetTexture(m_textures.KeyAt(index), texture);
}

void Material::SetTextures(const TextureSet& textures)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting textures on static material with Id {} (name: {})", GetID(), GetName());
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    if (m_textures == textures)
    {
        return;
    }

    m_textures = textures;

    if (IsInitCalled())
    {
        for (SizeType i = 0; i < m_textures.Size(); i++)
        {
            if (!m_textures.ValueAt(i).IsValid())
            {
                continue;
            }

            InitObject(m_textures.ValueAt(i));
        }

        if (!g_render_backend->GetRenderConfig().IsBindlessSupported())
        {
            FlatMap<MaterialTextureKey, Handle<Texture>> textures;

            for (SizeType i = 0; i < m_textures.Size(); i++)
            {
                MaterialTextureKey key = m_textures.KeyAt(i);

                if (Handle<Texture>& texture = m_textures.ValueAt(i))
                {
                    textures.Set(key, texture);
                }
            }

            m_render_resource->SetTextures(std::move(textures));
        }

        m_mutation_state |= DataMutationState::DIRTY;
    }
}

const Handle<Texture>& Material::GetTexture(MaterialTextureKey key) const
{
    return m_textures.Get(key);
}

const Handle<Texture>& Material::GetTextureAtIndex(uint32 index) const
{
    return GetTexture(m_textures.KeyAt(index));
}

Handle<Material> Material::Clone() const
{
    Handle<Material> material = CreateObject<Material>(
        Name::Unique(ANSIString(*m_name) + "_dynamic"),
        m_render_attributes,
        m_parameters,
        m_textures);

    // cloned materials are dynamic by default
    material->m_is_dynamic = true;

    return material;
}

HashCode Material::GetHashCode() const
{
    HashCode hc;

    hc.Add(m_parameters.GetHashCode());
    hc.Add(m_textures.GetHashCode());
    hc.Add(m_render_attributes.GetHashCode());

    return hc;
}

#pragma endregion Material

#pragma region MaterialGroup

MaterialGroup::MaterialGroup()
    : HypObject()
{
}

MaterialGroup::~MaterialGroup()
{
}

void MaterialGroup::Init()
{
    for (auto& it : m_materials)
    {
        InitObject(it.second);
    }

    SetReady(true);
}

void MaterialGroup::Add(const String& name, Handle<Material>&& material)
{
    if (IsInitCalled())
    {
        InitObject(material);
    }

    m_materials[name] = std::move(material);
}

bool MaterialGroup::Remove(const String& name)
{
    const auto it = m_materials.Find(name);

    if (it != m_materials.End())
    {
        m_materials.Erase(it);

        return true;
    }

    return false;
}

#pragma endregion MaterialGroup

#pragma region MaterialCache

MaterialCache* MaterialCache::GetInstance()
{
    return g_material_system;
}

void MaterialCache::Add(const Handle<Material>& material)
{
    if (!material)
    {
        return;
    }

    AssertThrowMsg(!material->IsDynamic(), "Cannot add dynamic material to cache, as changes to the material will affect all instances");

    Mutex::Guard guard(m_mutex);

    HashCode hc;
    hc.Add(material->GetRenderAttributes().GetHashCode());
    hc.Add(material->GetParameters().GetHashCode());
    hc.Add(material->GetTextures().GetHashCode());

    m_map.Set(hc, material);
}

Handle<Material> MaterialCache::CreateMaterial(
    Name name,
    MaterialAttributes attributes,
    const Material::ParameterTable& parameters,
    const Material::TextureSet& textures)
{
    if (!attributes.shader_definition)
    {
        attributes.shader_definition = ShaderDefinition {
            NAME("Forward"),
            static_mesh_vertex_attributes
        };
    }

    Handle<Material> handle = CreateObject<Material>(
        name,
        attributes,
        parameters,
        textures);

    InitObject(handle);

    return handle;
}

Handle<Material> MaterialCache::GetOrCreate(
    Name name,
    MaterialAttributes attributes,
    const Material::ParameterTable& parameters,
    const Material::TextureSet& textures)
{
    if (!attributes.shader_definition)
    {
        attributes.shader_definition = ShaderDefinition {
            NAME("Forward"),
            static_mesh_vertex_attributes
        };
    }

    // @TODO: For textures hashcode, asset path should be used rather than texture Id
    // textures may later be destroyed and their IDs reused which would cause a hash collision

    HashCode hc;
    hc.Add(attributes.GetHashCode());
    hc.Add(parameters.GetHashCode());
    hc.Add(textures.GetHashCode());

    Handle<Material> handle;

    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_map.FindByHashCode(hc);

        if (it != m_map.End())
        {
            if (Handle<Material> handle = it->second.Lock())
            {
                return handle;
            }
        }

        if (!name.IsValid())
        {
            name = Name::Unique(ANSIString("cached_material_") + ANSIString::ToString(hc.Value()));
        }

        handle = CreateObject<Material>(
            name,
            attributes,
            parameters,
            textures);

        m_map.Set(hc, handle);
    }

    AssertThrow(!handle->IsDynamic());
    InitObject(handle);

    return handle;
}

#pragma region MaterialCache

} // namespace hyperion
