/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>

#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderConfig.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <core/Types.hpp>

namespace hyperion {

static const ShaderDefinition g_defaultShaderDefinition {
    NAME("Forward"),
    ShaderProperties(staticMeshVertexAttributes)
};

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
    : m_renderAttributes {
          .shaderDefinition = g_defaultShaderDefinition,
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
    : m_name(name),
      m_renderAttributes {
          .shaderDefinition = g_defaultShaderDefinition,
          .bucket = rb
      },
      m_isDynamic(false),
      m_mutationState(DataMutationState::CLEAN),
      m_renderProxyVersion(0)
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
      m_renderAttributes(attributes),
      m_isDynamic(false),
      m_mutationState(DataMutationState::CLEAN),
      m_renderProxyVersion(0)
{
}

Material::~Material()
{
    SetReady(false);

    for (SizeType i = 0; i < m_textures.Size(); i++)
    {
        m_textures.ValueAt(i).Reset();
    }
}

void Material::Init()
{
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

    m_mutationState |= DataMutationState::DIRTY;

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

void Material::SetParameter(MaterialKey key, const Parameter& value)
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

    m_parameters.Set(key, value);

    if (IsInitCalled())
    {
        m_mutationState |= DataMutationState::DIRTY;

        SetNeedsRenderProxyUpdate();
    }
}

void Material::SetParameters(const ParameterTable& parameters)
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

    m_textures.Set(key, texture);

    if (IsInitCalled())
    {
        InitObject(texture);

        SetNeedsRenderProxyUpdate();

        m_mutationState |= DataMutationState::DIRTY;
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
        HYP_LOG(Material, Warning, "Setting textures on static material with Id {} (name: {})", Id(), GetName());
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

        SetNeedsRenderProxyUpdate();

        m_mutationState |= DataMutationState::DIRTY;
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
        m_renderAttributes,
        m_parameters,
        m_textures);

    // cloned materials are dynamic by default
    material->m_isDynamic = true;

    return material;
}

void Material::UpdateRenderProxy(RenderProxyMaterial* proxy)
{
    proxy->material = WeakHandleFromThis();

    static const bool useBindlessTextures = g_renderBackend->GetRenderConfig().IsBindlessSupported();

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

    const uint32 numTextureSlots = MathUtil::Min(maxTextures, useBindlessTextures ? g_maxBindlessResources : g_maxBoundTextures);
    uint32 remainingTextureSlots = numTextureSlots;

    proxy->boundTextures.Clear();

    // unset all bound texture indices
    for (uint32 i = 0; i < g_maxBoundTextures; ++i)
    {
        proxy->boundTextureIndices[i] = ~0u;
    }

    for (const auto& it : m_textures)
    {
        if (remainingTextureSlots == 0)
        {
            break;
        }

        const uint32 slot = MathUtil::FastLog2_Pow2(uint32(it.first));
        const Handle<Texture>& texture = it.second;

        if (texture.IsValid())
        {
            AssertDebug(slot < g_maxBoundTextures);

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
            bufferData.textureUsage |= 1u << slot;

            proxy->boundTextureIndices[slot] = idx;

            --remainingTextureSlots;
        }
    }
}

HashCode Material::GetHashCode() const
{
    HashCode hc;

    hc.Add(m_parameters.GetHashCode());
    hc.Add(m_textures.GetHashCode());
    hc.Add(m_renderAttributes.GetHashCode());

    return hc;
}

#pragma endregion Material

#pragma region MaterialGroup

MaterialGroup::MaterialGroup()
    : HypObjectBase()
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
    const Material::ParameterTable& parameters,
    const Material::TextureSet& textures)
{
    if (!attributes.shaderDefinition)
    {
        attributes.shaderDefinition = g_defaultShaderDefinition;
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
    if (!attributes.shaderDefinition)
    {
        attributes.shaderDefinition = g_defaultShaderDefinition;
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
