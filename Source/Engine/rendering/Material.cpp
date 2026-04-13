/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/utilities/ByteUtil.hpp>

#include <engine/EngineDriver.hpp>

#include <Material.generated.inl>

namespace Hyperion {

static const Name s_defaultShaderName = NAME("GeometryPass");

static HashCode GetMaterialHashCode(
    const MaterialAttributes& attributes,
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
{
    HashCode hc;
    hc.Add(attributes.GetHashCode());
    hc.Add(parameters.GetHashCode());

    // For textures, we use the asset path of each texture for hashing rather than
    // runtime dynamic ID so it is stable and doesn't rely on texture instances to be the same.
    for (Texture* tex : textures)
    {
        if (!tex)
        {
            continue;
        }

        hc.Add(tex->GetPath().GetHashCode());
    }

    return hc;
}

#pragma region Material

const StringHash Material::s_textureNames[] = {
    "DiffuseMap"_sh,
    "NormalMap"_sh,
    "ParallaxMap"_sh,
    "MetalnessMap"_sh,
    "RoughnessMap"_sh,
    "AoMap"_sh
};

Material::Material()
    : m_attributes {
          .shaderName = s_defaultShaderName,
          .bucket = RenderBucket::Opaque,
          .fillMode = FM_FILL,
          .blendFunction = BlendFunction::None(),
          .cullFaces = FCM_BACK,
          .flags = MAF_DEPTH_WRITE | MAF_DEPTH_TEST
      },
      m_isDynamic(false),
      m_renderProxyVersion(0)
{
    ResetParameters();
}

Material::Material(Name name, RenderBucket rb)
    : AssetObject(name),
      m_attributes {
          .shaderName = s_defaultShaderName,
          .bucket = rb
      },
      m_isDynamic(false),
      m_renderProxyVersion(0)
{
    ResetParameters();
}

Material::Material(Name name, const MaterialAttributes& attributes)
    : Material(name, attributes, MaterialParameters {}, MaterialTextures {})
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
      m_renderProxyVersion(0)
{
    UpdateAttributesTextureMask();
}

Material::~Material()
{
    SetReady(false);

    for (size_t i = 0; i < m_textures.Size(); i++)
    {
        Handle<Texture>& texture = m_textures.AtIndex(i);

        if (texture)
        {
            EnqueueDeletion(std::move(texture));
        }
    }
}

void Material::Init()
{
    HYP_SCOPE;

    for (size_t i = 0; i < m_textures.Size(); i++)
    {
        Pair<MaterialTextureKey, Handle<Texture>&> keyValue = m_textures.KeyValueAt(i);

        const Handle<Texture>& texture = keyValue.second;

        if (!texture.IsValid())
        {
            continue;
        }

        CheckResult(keyValue.second->Create());
    }

    AssetObject::Init();

    SetReady(true);

    EnqueueRenderUpdates();
}

void Material::EnqueueRenderUpdates()
{
    AssertReady();

    SetNeedsRenderProxyUpdate();
}

void Material::SetParameters(const MaterialParameters& parameters)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting parameters on static material with Id {} (name: {})", Id(), GetName());
#if HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    m_parameters = parameters;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Material::ResetParameters()
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Resetting parameters on static material with Id {} (name: {})", Id(), GetName());
#if HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    m_parameters = MaterialParameters {};

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Material::SetTexture(MaterialTextureKey key, const Handle<Texture>& texture)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting texture on static material with Id {} (name: {})", Id(), GetName());
#if HYP_DEBUG_MODE
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
        EnqueueDeletion(std::move(m_textures[key]));
    }

    m_textures[key] = texture;

    UpdateAttributesTextureMask();

    CheckResult(texture->Create());

    SetNeedsRenderProxyUpdate();
    MarkDirty();
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
#if HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif // HYP_DEBUG_MODE
    }

    if (m_textures == textures)
    {
        return;
    }

    for (size_t i = 0; i < m_textures.Size(); i++)
    {
        Handle<Texture>& texture = m_textures.AtIndex(i);

        if (texture != nullptr)
        {
            EnqueueDeletion(std::move(texture));
        }
    }

    m_textures = textures;

    UpdateAttributesTextureMask();

    for (size_t i = 0; i < m_textures.Size(); i++)
    {
        if (!m_textures.AtIndex(i).IsValid())
        {
            continue;
        }

        CheckResult(m_textures.AtIndex(i)->Create());
    }

    SetNeedsRenderProxyUpdate();

    MarkDirty();
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
    Handle<Material> material = MakeHandle<Material>(
        GetName(),
        m_attributes,
        m_parameters,
        m_textures);

    // cloned materials are dynamic by default
    material->m_isDynamic = true;

    return material;
}

void Material::UpdateRenderProxy(RenderProxyMaterial* proxy)
{    
    const bool useBindlessTextures = g_renderInterface->GetRenderConfig().bindlessTextures;

    if (proxy->material.GetUnsafe() != this)
    {
        proxy->material = MakeWeakRef(this);
    }

    proxy->attributes = m_attributes;
    
    MaterialShaderData& bufferData = proxy->bufferData;

    bufferData.albedo = m_parameters.albedo;
    bufferData.packedParams = Vec4u(
        ByteUtil::PackVec4f(Vec4f {
            m_parameters.roughness,
            m_parameters.metalness,
            m_parameters.transmission,
            1.0f
        }),
        ByteUtil::PackVec4f(Vec4f {
            m_parameters.emissiveColor.GetXYZ() * m_parameters.emissiveColor.w,
            m_parameters.alphaThreshold
        }),
        ByteUtil::PackVec4f(Vec4f::Zero()),
        ByteUtil::PackVec4f(Vec4f::Zero()));
    bufferData.uvScale = 1.0f;
    bufferData.parallaxHeight = m_parameters.parallaxHeightScale;

    bufferData.textureUsage = 0;

    uint32* textureIndicesU32 = reinterpret_cast<uint32*>(bufferData.textureIndices);
    Memory::Fill(textureIndicesU32, 0, sizeof(bufferData.textureIndices));

    const uint32 numTextureSlots = MathUtil::Min(
        MaterialTextures::MaxTextures, useBindlessTextures ? MaxBindlessResources[BindlessStorage_Textures] : MaxBoundTextures);

    uint32 remainingTextureSlots = numTextureSlots;

    proxy->boundTextures.Clear();

    // unset all bound texture indices (~0u)
    Memory::Fill(&proxy->boundTextureIndices[0], 0xFF, sizeof(proxy->boundTextureIndices));

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
    return GetMaterialHashCode(m_attributes, m_parameters, m_textures);
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
    InitObject(material);

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
    return g_materialCache;
}

void MaterialCache::Add(const Handle<Material>& material)
{
    if (!material)
    {
        return;
    }

    Assert(!material->GetIsDynamic(), "Cannot add dynamic material to cache, as changes to the material will affect all instances");

    Mutex::Guard guard(m_mutex);

    const HashCode hc = GetMaterialHashCode(
        material->GetRenderAttributes(),
        material->GetParameters(),
        material->GetTextures());

    m_map.Set(hc, material);
}

Handle<Material> MaterialCache::CreateMaterial(
    Name name,
    const MaterialAttributes& attributes,
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
{
    MaterialAttributes tmpAttributes;
    const MaterialAttributes* attributesPtr = &attributes;

    if (!attributes.shaderName)
    {
        tmpAttributes = attributes;
        tmpAttributes.shaderName = s_defaultShaderName;

        attributesPtr = &tmpAttributes;
    }

    Handle<Material> handle = MakeHandle<Material>(
        name,
        *attributesPtr,
        parameters,
        textures);

    InitObject(handle);

    return handle;
}

Handle<Material> MaterialCache::GetOrCreate(
    Name name,
    const MaterialAttributes& attributes,
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
{
    const MaterialAttributes* attributesPtr = &attributes;
    MaterialAttributes tmpAttributes;

    if (!attributes.shaderName)
    {
        tmpAttributes = attributes;
        tmpAttributes.shaderName = s_defaultShaderName;

        attributesPtr = &tmpAttributes;
    }

    const HashCode hc = GetMaterialHashCode(*attributesPtr, parameters, textures);

    Handle<Material> strongRef;

    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_map.FindByHashCode(hc);

        if (it != m_map.End())
        {
            strongRef = MakeStrongRef(it->second);

            if (strongRef != nullptr)
            {
                return strongRef;
            }
        }

        if (!name.IsValid())
        {
            name = Name::Unique(ANSIString("cached_material_") + ANSIString::ToString(hc.Value()));
        }

        strongRef = MakeHandle<Material>(
            name,
            *attributesPtr,
            parameters,
            textures);

        m_map.Set(hc, strongRef);
    }

    Assert(!strongRef->GetIsDynamic());
    InitObject(strongRef);

    return strongRef;
}

#pragma region MaterialCache

} // namespace Hyperion
