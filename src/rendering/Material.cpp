/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderConfig.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <Material.generated.inl>

namespace hyperion {

static const ShaderDefinition s_defaultShaderDefinition {
    NAME("GeometryPass"),
    ShaderProperties(staticMeshVertexAttributes)
};

#pragma region MaterialParameter

MaterialParameter::SerializedValueType MaterialParameter::SerializeData() const
{
    switch (type)
    {
    case MPT_FLOAT:
        return SerializedValueType(float32(*this));
    case MPT_FLOAT2:
        return SerializedValueType(Vec2f(*this));
    case MPT_FLOAT3:
        return SerializedValueType(Vec3f(*this));
    case MPT_FLOAT4:
        return SerializedValueType(Vec4f(*this));
    case MPT_INT:
        return SerializedValueType(int32(*this));
    case MPT_INT2:
        return SerializedValueType(Vec2i(*this));
    case MPT_INT3:
        return SerializedValueType(Vec3i(*this));
    case MPT_INT4:
        return SerializedValueType(Vec4i(*this));
    default:
        return SerializedValueType();
    }
}

void MaterialParameter::DeserializeData(const SerializedValueType& data)
{
    if (data.Is<float32>())
    {
        *this = MaterialParameter(data.Get<float32>());
    }
    else if (data.Is<Vec2f>())
    {
        *this = MaterialParameter(data.Get<Vec2f>());
    }
    else if (data.Is<Vec3f>())
    {
        *this = MaterialParameter(data.Get<Vec3f>());
    }
    else if (data.Is<Vec4f>())
    {
        *this = MaterialParameter(data.Get<Vec4f>());
    }
    else if (data.Is<int32>())
    {
        *this = MaterialParameter(data.Get<int32>());
    }
    else if (data.Is<Vec2i>())
    {
        *this = MaterialParameter(data.Get<Vec2i>());
    }
    else if (data.Is<Vec3i>())
    {
        *this = MaterialParameter(data.Get<Vec3i>());
    }
    else if (data.Is<Vec4i>())
    {
        *this = MaterialParameter(data.Get<Vec4i>());
    }
    else
    {
        type = MPT_NONE;
        Memory::MemSet(&value, 0, sizeof(value));
    }
}

#pragma endregion MaterialParameter

#pragma region Material

const Array<Name> Material::s_textureNames = {
    NAME("AlbedoMap"),
    NAME("NormalMap"),
    NAME("ParallaxMap"),
    NAME("MetalnessMap"),
    NAME("RoughnessMap"),
    NAME("AoMap")
};

const MaterialParameters& Material::DefaultParameters()
{
    static const MaterialParameters s_defaultParameters {
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

    return s_defaultParameters;
}

Material::Material()
    : m_attributes {
          .shaderDefinition = s_defaultShaderDefinition,
          .bucket = RB_OPAQUE,
          .fillMode = FM_FILL,
          .blendFunction = BlendFunction::None(),
          .cullFaces = FCM_BACK,
          .flags = MAF_DEPTH_WRITE | MAF_DEPTH_TEST
      },
      m_isDynamic(false),
      m_mutationState(DataMutationState::CLEAN),
      m_renderProxyVersion(0)
{
    ResetParameters();
}

Material::Material(Name name, RenderBucket rb)
    : AssetObject(name),
      m_attributes {
          .shaderDefinition = s_defaultShaderDefinition,
          .bucket = rb
      },
      m_isDynamic(false),
      m_mutationState(DataMutationState::CLEAN),
      m_renderProxyVersion(0)
{
    ResetParameters();
}

Material::Material(Name name, const MaterialAttributes& attributes)
    : Material(name, attributes, DefaultParameters(), MaterialTextures {})
{
}

Material::Material(
    Name name,
    const MaterialAttributes& attributes,
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
    : AssetObject(name),
      m_parameters(parameters),
      m_textures(textures),
      m_attributes(attributes),
      m_isDynamic(false),
      m_mutationState(DataMutationState::CLEAN),
      m_renderProxyVersion(0)
{
    UpdateAttributesTextureMask();
}

Material::~Material()
{
    SetReady(false);

    for (SizeType i = 0; i < m_textures.Size(); i++)
    {
        Handle<Texture>& texture = m_textures.AtIndex(i);

        if (texture)
        {
            SafeDelete(std::move(texture));
        }
    }
}

void Material::Init()
{
    HYP_SCOPE;

    for (SizeType i = 0; i < m_textures.Size(); i++)
    {
        Pair<MaterialTextureKey, Handle<Texture>&> keyValue = m_textures.KeyValueAt(i);

        InitObject(keyValue.second);
    }

    m_mutationState |= DataMutationState::DIRTY;

    AssetObject::Init();

    SetReady(true);

    EnqueueRenderUpdates();
}

void Material::EnqueueRenderUpdates()
{
    AssertReady();

    if (!m_mutationState.IsDirty())
    {
        HYP_LOG_ONCE(Material, Warning, "EnqueueRenderUpdates called on material with Id {} (name: {}) that is not dirty", Id(), GetName());

        return;
    }

    SetNeedsRenderProxyUpdate();

    m_mutationState = DataMutationState::CLEAN;
}

void Material::SetParameter(MaterialParameterKey key, const MaterialParameter& value)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting parameter on static material with Id {} (name: {})", Id(), GetName());
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    if (m_parameters[key] == value)
    {
        return;
    }

    m_parameters[key] = value;

    if (IsInitCalled())
    {
        m_mutationState |= DataMutationState::DIRTY;

        SetNeedsRenderProxyUpdate();
    }
}

void Material::SetParameters(const MaterialParameters& parameters)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting parameters on static material with Id {} (name: {})", Id(), GetName());
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    m_parameters = parameters;

    if (IsInitCalled())
    {
        m_mutationState |= DataMutationState::DIRTY;

        SetNeedsRenderProxyUpdate();
    }
}

void Material::ResetParameters()
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Resetting parameters on static material with Id {} (name: {})", Id(), GetName());
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    m_parameters = DefaultParameters();

    if (IsInitCalled())
    {
        m_mutationState |= DataMutationState::DIRTY;

        SetNeedsRenderProxyUpdate();
    }
}

void Material::SetTexture(MaterialTextureKey key, const Handle<Texture>& texture)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting texture on static material with Id {} (name: {})", Id(), GetName());
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    if (m_textures[key] == texture)
    {
        return;
    }

    if (m_textures[key] != nullptr)
    {
        // if the texture is already set, delete it
        SafeDelete(std::move(m_textures[key]));
    }

    m_textures[key] = texture;

    UpdateAttributesTextureMask();

    if (IsInitCalled())
    {
        InitObject(texture);

        SetNeedsRenderProxyUpdate();

        m_mutationState |= DataMutationState::DIRTY;
    }
}

void Material::SetTextureAtIndex(uint32 index, const Handle<Texture>& texture)
{
    return SetTexture(m_textures.KeyValueAt(index).first, texture);
}

void Material::SetTextures(const MaterialTextures& textures)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting textures on static material with id {} (name: {})", Id(), GetName());
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    if (m_textures == textures)
    {
        return;
    }

    for (SizeType i = 0; i < m_textures.Size(); i++)
    {
        Handle<Texture>& texture = m_textures.AtIndex(i);

        if (texture != nullptr)
        {
            SafeDelete(std::move(texture));
        }
    }

    m_textures = textures;

    UpdateAttributesTextureMask();

    if (IsInitCalled())
    {
        for (SizeType i = 0; i < m_textures.Size(); i++)
        {
            if (!m_textures.AtIndex(i).IsValid())
            {
                continue;
            }

            InitObject(m_textures.AtIndex(i));
        }

        SetNeedsRenderProxyUpdate();

        m_mutationState |= DataMutationState::DIRTY;
    }
}

const Handle<Texture>& Material::GetTexture(MaterialTextureKey key) const
{
    return m_textures[key];
}

const Handle<Texture>& Material::GetTextureAtIndex(uint32 index) const
{
    return m_textures.AtIndex(index);
}

Handle<Material> Material::Clone() const
{
    Handle<Material> material = CreateObject<Material>(
        Name::Unique(ANSIString(*m_name) + "_dynamic"),
        m_attributes,
        m_parameters,
        m_textures);

    // cloned materials are dynamic by default
    material->m_isDynamic = true;

    return material;
}

void Material::UpdateRenderProxy(RenderProxyMaterial* proxy)
{
    if (proxy->material.GetUnsafe() != this)
    {
        proxy->material = MakeWeakRef(this);
    }

    const bool useBindlessTextures = g_renderBackend->GetRenderConfig().bindlessTextures;

    MaterialShaderData& bufferData = proxy->bufferData;

    bufferData.albedo = GetParameter<Vec4f>(MATERIAL_KEY_ALBEDO);
    bufferData.packedParams = Vec4u(
        ByteUtil::PackVec4f(Vec4f(
            GetParameter<float>(MATERIAL_KEY_ROUGHNESS),
            GetParameter<float>(MATERIAL_KEY_METALNESS),
            GetParameter<float>(MATERIAL_KEY_TRANSMISSION),
            GetParameter<float>(MATERIAL_KEY_NORMAL_MAP_INTENSITY))),
        ByteUtil::PackVec4f(Vec4f(GetParameter<float>(MATERIAL_KEY_ALPHA_THRESHOLD))),
        ByteUtil::PackVec4f(Vec4f {}),
        ByteUtil::PackVec4f(Vec4f {}));
    bufferData.uvScale = GetParameter<Vec2f>(MATERIAL_KEY_UV_SCALE);
    bufferData.parallaxHeight = GetParameter<float>(MATERIAL_KEY_PARALLAX_HEIGHT);

    bufferData.textureUsage = 0;

    uint32* textureIndicesU32 = reinterpret_cast<uint32*>(&bufferData.textureIndices);
    Memory::MemSet(textureIndicesU32, 0, sizeof(bufferData.textureIndices));

    const uint32 numTextureSlots = MathUtil::Min(MaterialTextures::MaxTextures, useBindlessTextures ? MaxBindlessResources : MaxBoundTextures);
    uint32 remainingTextureSlots = numTextureSlots;

    proxy->boundTextures.Clear();

    // unset all bound texture indices (~0u)
    Memory::MemSet(&proxy->boundTextureIndices[0], 0xFF, sizeof(proxy->boundTextureIndices));

    for (uint32 slot = 0; slot < uint32(m_textures.Size()); slot++)
    {
        if (remainingTextureSlots == 0)
        {
            break;
        }

        const Handle<Texture>& texture = m_textures.AtIndex(slot);

        if (texture != nullptr)
        {
            const uint32 idx = uint32(proxy->boundTextures.Size());
            proxy->boundTextures.PushBack(texture);

            if (useBindlessTextures)
            {
                textureIndicesU32[slot] = texture.Id().ToIndex();
            }
            else
            {
                textureIndicesU32[slot] = idx;
            }

            // enable this slot for the texture
            bufferData.textureUsage |= (1u << slot);

            proxy->boundTextureIndices[slot] = idx;

            --remainingTextureSlots;
        }
    }
}

void Material::UpdateAttributesTextureMask()
{
    m_attributes.textureMask = 0;

    for (uint32 i = 0; i < uint32(m_textures.Size()); i++)
    {
        if (m_textures.AtIndex(i) != nullptr)
        {
            m_attributes.textureMask |= (1u << i);
        }
    }
}

HashCode Material::GetHashCode() const
{
    HashCode hc;

    hc.Add(m_parameters.GetHashCode());
    hc.Add(m_textures.GetHashCode());
    hc.Add(m_attributes.GetHashCode());

    return hc;
}

#pragma endregion Material

#pragma region MaterialGroup

MaterialGroup::MaterialGroup()
    : ObjectBase()
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
    return g_materialSystem;
}

void MaterialCache::Add(const Handle<Material>& material)
{
    if (!material)
    {
        return;
    }

    Assert(!material->IsDynamic(), "Cannot add dynamic material to cache, as changes to the material will affect all instances");

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
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
{
    if (!attributes.shaderDefinition)
    {
        attributes.shaderDefinition = s_defaultShaderDefinition;
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
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
{
    if (!attributes.shaderDefinition)
    {
        attributes.shaderDefinition = s_defaultShaderDefinition;
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

    Assert(!handle->IsDynamic());
    InitObject(handle);

    return handle;
}

#pragma region MaterialCache

} // namespace hyperion
