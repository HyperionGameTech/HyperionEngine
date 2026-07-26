/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/Bindless.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Framework/EngineDriver.hpp>

#include <Material.generated.inl>

namespace Hyperion {

static const Name s_defaultShaderName = NAME("GeometryPass");

static HashCode GetMaterialHashCode(
    const Material* base,
    const MaterialAttributes& attributes,
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
{
    HashCode hc;

    if (base)
    {
        hc.Add(base->GetHashCode());
    }
    else
    {
        hc.Add(attributes.GetHashCode());
    }

    hc.Add(parameters.GetHashCode());

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
    : m_isDynamic(false),
      m_renderProxyVersion(0)
{
}

Material::Material(Name name, RenderBucket rb)
    : AssetObject(name),
      m_attributes {
          .shaderName = s_defaultShaderName,
          .bucket = rb,
          .fillMode = FM_FILL,
          .blendFunction = BlendFunction::None(),
          .cullFaces = FCM_BACK,
          .flags = MAF_DEPTH_WRITE | MAF_DEPTH_TEST
      },
      m_isDynamic(false),
      m_renderProxyVersion(0)
{
}

Material::Material(Name name, const MaterialAttributes& attributes)
    : Material(name, attributes, MaterialParameters {}, MaterialTextures {})
{
}

Material::Material(Name name, const Handle<Material>& base)
    : AssetObject(name),
      m_base(base.Get() != this ? base : Handle<Material>::Null()),
      m_isDynamic(false),
      m_renderProxyVersion(0)
{
    if (m_base)
    {
        m_parameters = m_base->GetParameters();
        m_textures = m_base->GetTextures();
    }
}

Material::Material(
    Name name,
    const MaterialAttributes& attributes,
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
    : AssetObject(name),
      m_attributes(attributes),
      m_parameters(parameters),
      m_textures(textures),
      m_isDynamic(false),
      m_renderProxyVersion(0)
{
    if (!m_attributes.shaderName)
    {
        m_attributes.shaderName = s_defaultShaderName;
    }
}

Material::Material(
    Name name,
    const Handle<Material>& base,
    const MaterialAttributes& attributes,
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
    : AssetObject(name),
      m_base(base.Get() != this ? base : Handle<Material>::Null()),
      m_attributes(attributes),
      m_parameters(parameters),
      m_textures(textures),
      m_isDynamic(false),
      m_renderProxyVersion(0)
{
    if (!m_attributes.shaderName)
    {
        m_attributes.shaderName = s_defaultShaderName;
    }
}

Material::~Material()
{
    LockWriter();

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
    if (m_base.IsValid())
    {
        const bool isCircularRef = (m_base.Get() == this || m_base->GetBaseMaterial().Get() == this);

        Assert(!isCircularRef,
               "Circular reference between material and base material detected! Would deadlock!");

        if (isCircularRef)
        {
            // Release it, or else we'll never be destroyed
            m_base.Reset();
        }
        else
        {

            InitObject(m_base);
        }
    }

    for (size_t i = 0; i < m_textures.Size(); i++)
    {
        Pair<MaterialTextureKey, Handle<Texture>&> keyValue = m_textures.KeyValueAt(i);

        const Handle<Texture>& texture = keyValue.second;

        if (!texture.IsValid())
        {
            continue;
        }

        Check(keyValue.second->Create());
    }

    AssetObject::Init();

    SetNeedsRenderProxyUpdate();

    SetReady(true);
}

void Material::SetBaseMaterial(const Handle<Material>& baseMaterial)
{
    if (m_base == baseMaterial)
    {
        return;
    }

    if (baseMaterial.IsValid())
    {
        if (baseMaterial.Get() == this || baseMaterial->GetBaseMaterial().Get() == this)
        {
            // would cause circular reference!
            // return to prevent that from happening.
            return;
        }
    }

    m_base = baseMaterial;

    MarkDirty();
    SetNeedsRenderProxyUpdate();
}

const MaterialAttributes& Material::GetAttributes() const
{
    if (m_base)
    {
        return m_base->GetAttributes();
    }

    return m_attributes;
}

MaterialAttributes& Material::GetAttributes()
{
    if (m_base)
    {
        return m_base->GetAttributes();
    }

    return m_attributes;
}

void Material::SetParameters(const MaterialParameters& parameters)
{
    if (parameters == m_parameters)
    {
        return;
    }

    m_parameters = parameters;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Material::ResetParameters()
{
    if (m_base.IsValid())
    {
        m_parameters = m_base->GetParameters();
    }
    else
    {
        m_parameters = MaterialParameters {};
    }

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Material::SetTexture(MaterialTextureKey key, const Handle<Texture>& texture)
{
    if (IsStatic())
    {
        HYP_LOG(Material, Warning, "Setting texture on static material with Id {} (name: {})", Id(), GetName());
    }

    auto textureIt = m_textures.Find(key);
    if (textureIt != m_textures.End())
    {
        if (*textureIt == texture)
        {
            return;
        }

        if (textureIt->IsValid())
        {
            EnqueueDeletion(std::move(*textureIt));
        }

        *textureIt = texture;
    }
    else
    {
        m_textures[key] = texture;
    }

    Check(texture->Create());

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void Material::SetTextureAtIndex(uint32 index, const Handle<Texture>& texture)
{
    return SetTexture(m_textures.KeyValueAt(index).first, texture);
}

void Material::SetTextures(const MaterialTextures& textures)
{
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

    for (size_t i = 0; i < m_textures.Size(); i++)
    {
        if (!m_textures.AtIndex(i).IsValid())
        {
            continue;
        }

        Check(m_textures.AtIndex(i)->Create());
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
    Handle<Material> clone = MakeHandle<Material>(
        GetName(),
        m_base,
        m_attributes,
        m_parameters,
        m_textures);

    clone->m_isDynamic = true;

    return clone;
}

void Material::UpdateRenderProxy(RenderProxyMaterial* proxy)
{
    const bool useBindlessTextures = RI.GetRenderConfig().bindlessTextures;

    proxy->material = this;

    proxy->attributes = GetAttributes();

    MaterialShaderData& bufferData = proxy->bufferData;
    bufferData = {};

    bufferData.albedo = m_parameters.albedo;
    bufferData.packedParams = Vec4u(
        ByteUtil::PackVec4f(Vec4f {
            m_parameters.roughness,
            m_parameters.metalness,
            m_parameters.transmission,
            m_parameters.alphaThreshold }),
        ByteUtil::PackVec4f(Vec4f {
            m_parameters.emissiveColor.GetRed(),
            m_parameters.emissiveColor.GetGreen(),
            m_parameters.emissiveColor.GetBlue(),
            m_parameters.emissiveIntensity }),
        0, 0);

    union
    {
        uint32 bits;

        struct
        {
            uint32 unlit : 1;
            uint32 normalMapFlipY : 1;
            uint32 roughnessChannel : 2;
            uint32 metalnessChannel : 2;
            uint32 aoChannel : 2;
            uint32 parallaxInverseHeight : 1;
        };
    } flags;

    flags.bits = 0;
    flags.unlit = static_cast<uint32>(m_parameters.unlit);
    flags.normalMapFlipY = static_cast<uint32>(m_parameters.IsNormalMapFlipY());
    flags.roughnessChannel = static_cast<uint32>(m_parameters.GetRoughnessChannel());
    flags.metalnessChannel = static_cast<uint32>(m_parameters.GetMetalnessChannel());
    flags.aoChannel = static_cast<uint32>(m_parameters.GetAmbientOcclusionChannel());
    flags.parallaxInverseHeight = static_cast<uint32>(m_parameters.IsParallaxInverseHeight());

    bufferData.packedParams.w = flags.bits;

    bufferData.uvScale = m_parameters.uvScale;
    bufferData.parallaxHeight = m_parameters.parallaxHeightScale;

    bufferData.textureUsage = 0;

    uint32* textureIndicesU32 = reinterpret_cast<uint32*>(bufferData.textureIndices);
    Memory::Zero(textureIndicesU32, sizeof(bufferData.textureIndices));

    const uint32 numTextureSlots = MathUtil::Min(
        MaterialTextures::MaxTextures,
        useBindlessTextures ? MaxBindlessResources[BindlessStorage_Textures] : MaxBoundTextures);

    uint32 remainingTextureSlots = numTextureSlots;

    Memory::Fill(&proxy->boundTextureIndices[0], 0xFFu, sizeof(proxy->boundTextureIndices));

    proxy->boundTextures.Clear();

    for (uint32 slot = 0; slot < uint32(m_textures.Size()); slot++)
    {
        if (remainingTextureSlots == 0)
        {
            break;
        }

        Texture* texture = m_textures.AtIndex(slot);

        if (!texture && m_base.IsValid())
        {
            texture = m_base->m_textures.AtIndex(slot);
        }

        if (texture != nullptr)
        {
            const uint32 idx = uint32(proxy->boundTextures.Size());
            proxy->boundTextures.PushBack(texture);

            if (useBindlessTextures)
            {
                textureIndicesU32[slot] = texture->Id().ToIndex();
            }
            else
            {
                textureIndicesU32[slot] = idx;
            }

            bufferData.textureUsage |= (1u << slot);
            proxy->boundTextureIndices[slot] = idx;

            --remainingTextureSlots;
        }
    }
}

HashCode Material::GetHashCode() const
{
    return GetMaterialHashCode(m_base.Get(), m_attributes, m_parameters, m_textures);
}

#pragma endregion Material

#pragma region MaterialCache

void MaterialCache::Add(const Handle<Material>& material)
{
    if (!material)
    {
        return;
    }

    const HashCode hc = GetMaterialHashCode(
        material->GetBaseMaterial().Get(),
        material->GetAttributes(),
        material->GetParameters(),
        material->GetTextures());

    TUniqueLock lock(m_mutex);

    m_map.Set(hc, material);
}

Handle<Material> MaterialCache::Create(
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

    Handle<Material> material = MakeHandle<Material>(
        name,
        *attributesPtr,
        parameters,
        textures);

    GetCurrentAssetRegistry()->PutAsset(material);
    InitObject(material);

    return material;
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

    const HashCode hc = GetMaterialHashCode(nullptr, *attributesPtr, parameters, textures);

    Handle<Material> strongRef;

    {
        TSharedLock sharedLock(m_mutex);

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
            name = NAME_FMT("Mat_{}", hc.Value());
        }

        Handle<Material> material = MakeHandle<Material>(
            name,
            *attributesPtr,
            parameters,
            textures);

        material->SetIsTransient(true);

        strongRef = std::move(material);

        sharedLock.Reset();

        TUniqueLock uniqueLock(m_mutex);

        m_map.Set(hc, strongRef);
    }

    InitObject(strongRef);

    return strongRef;
}

#pragma endregion MaterialCache

} // namespace Hyperion
