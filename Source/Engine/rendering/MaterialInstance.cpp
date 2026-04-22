/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/MaterialInstance.hpp>
#include <rendering/MaterialDefinition.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Bindless.hpp>

#include <asset/AssetRegistry.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/utilities/ByteUtil.hpp>

#include <engine/EngineDriver.hpp>

#include <MaterialInstance.generated.inl>

namespace Hyperion {

#pragma region MaterialInstance

MaterialInstance::MaterialInstance()
    : m_isDynamic(false),
      m_renderProxyVersion(0)
{
}

MaterialInstance::MaterialInstance(Handle<MaterialDefinition> definition)
    : m_definition(std::move(definition)),
      m_isDynamic(false),
      m_renderProxyVersion(0)
{
}

MaterialInstance::MaterialInstance(Name name, Handle<MaterialDefinition> definition)
    : AssetObject(name),
      m_definition(std::move(definition)),
      m_isDynamic(false),
      m_renderProxyVersion(0)
{
}

MaterialInstance::MaterialInstance(
    Name name,
    Handle<MaterialDefinition> definition,
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
    : AssetObject(name),
      m_definition(std::move(definition)),
      m_parameters(parameters),
      m_textures(textures),
      m_isDynamic(false),
      m_renderProxyVersion(0)
{
}

MaterialInstance::~MaterialInstance()
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

    EnqueueDeletion(std::move(m_definition));
}

void MaterialInstance::Init()
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

const MaterialAttributes& MaterialInstance::GetAttributes() const
{
    Assert(m_definition != nullptr, "MaterialInstance has no definition set");

    return m_definition->GetAttributes();
}

RenderBucket MaterialInstance::GetBucket() const
{
    Assert(m_definition != nullptr, "MaterialInstance has no definition set");

    return m_definition->GetBucket();
}

void MaterialInstance::EnqueueRenderUpdates()
{
    AssertReady();

    SetNeedsRenderProxyUpdate();
}

void MaterialInstance::SetParameters(const MaterialParameters& parameters)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting parameters on static material instance with Id {} (name: {})", Id(), GetName());
    }

    m_parameters = parameters;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void MaterialInstance::ResetParameters()
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Resetting parameters on static material instance with Id {} (name: {})", Id(), GetName());
    }

    if (m_definition)
    {
        m_parameters = m_definition->GetDefaultParameters();
    }
    else
    {
        m_parameters = MaterialParameters {};
    }

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void MaterialInstance::SetTexture(MaterialTextureKey key, const Handle<Texture>& texture)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting texture on static material instance with Id {} (name: {})", Id(), GetName());
    }

    if (m_textures[key] == texture)
    {
        return;
    }

    if (m_textures[key] != nullptr)
    {
        EnqueueDeletion(std::move(m_textures[key]));
    }

    m_textures[key] = texture;

    CheckResult(texture->Create());

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void MaterialInstance::SetTextureAtIndex(uint32 index, const Handle<Texture>& texture)
{
    return SetTexture(m_textures.KeyValueAt(index).first, texture);
}

void MaterialInstance::SetTextures(const MaterialTextures& textures)
{
    if (IsStatic() && IsReady())
    {
        HYP_LOG(Material, Warning, "Setting textures on static material instance with id {} (name: {})", Id(), GetName());
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

const Handle<Texture>& MaterialInstance::GetTexture(MaterialTextureKey key) const
{
    return m_textures[key];
}

const Handle<Texture>& MaterialInstance::GetTextureAtIndex(uint32 index) const
{
    return m_textures.AtIndex(index);
}

Handle<MaterialInstance> MaterialInstance::Clone() const
{
    Assert(m_definition != nullptr, "MaterialInstance has no definition, cannot clone");

    Handle<MaterialInstance> instance = MakeHandle<MaterialInstance>(
        GetName(),
        m_definition,
        m_parameters,
        m_textures);

    instance->m_isDynamic = true;

    GetCurrentAssetRegistry()->PutAsset(instance);

    return instance;
}

void MaterialInstance::UpdateRenderProxy(RenderProxyMaterial* proxy)
{
    Assert(m_definition != nullptr, "MaterialInstance has no definition set");

    const bool useBindlessTextures = g_renderInterface->GetRenderConfig().bindlessTextures;

    if (proxy->material.GetUnsafe() != this)
    {
        proxy->material = MakeWeakRef(this);
    }

    proxy->attributes = m_definition->GetAttributes();

    MaterialShaderData& bufferData = proxy->bufferData;
    bufferData = {};

    bufferData.albedo = m_parameters.albedo;
    bufferData.packedParams = Vec4u(
        ByteUtil::PackVec4f(Vec4f {
            m_parameters.roughness,
            m_parameters.metalness,
            m_parameters.transmission,
            m_parameters.alphaThreshold
        }),
        ByteUtil::PackVec4f(Vec4f {
            m_parameters.emissiveColor.GetRed(),
            m_parameters.emissiveColor.GetGreen(),
            m_parameters.emissiveColor.GetBlue(),
            m_parameters.emissiveIntensity
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

    // unset all bound texture indices (~0u)
    Memory::Fill(&proxy->boundTextureIndices[0], 0xFFu, sizeof(proxy->boundTextureIndices));

    proxy->boundTextures.Clear();

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

            bufferData.textureUsage |= (1u << slot);
            proxy->boundTextureIndices[slot] = idx;

            --remainingTextureSlots;
        }
    }
}

HashCode MaterialInstance::GetHashCode() const
{
    HashCode hc;

    if (m_definition)
    {
        hc.Add(m_definition->GetHashCode());
    }

    hc.Add(m_parameters.GetHashCode());

    for (const Texture* tex : m_textures)
    {
        if (!tex)
        {
            continue;
        }

        hc.Add(tex->GetPath().GetHashCode());
    }

    return hc;
}

#pragma endregion MaterialInstance

} // namespace Hyperion
