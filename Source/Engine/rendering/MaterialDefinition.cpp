/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/MaterialDefinition.hpp>
#include <rendering/MaterialInstance.hpp>
#include <rendering/Texture.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <engine/EngineDriver.hpp>

#include <MaterialDefinition.generated.inl>

namespace Hyperion {

static const Name s_defaultDefinitionShaderName = NAME("GeometryPass");

static HashCode GetMaterialDefinitionHashCode(
    const MaterialAttributes& attributes,
    const MaterialParameters& parameters,
    const MaterialTextures& textures)
{
    HashCode hc;
    hc.Add(attributes.GetHashCode());
    hc.Add(parameters.GetHashCode());

    // Use asset paths for stable hashing independent of runtime texture IDs.
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

#pragma region MaterialDefinition

const StringHash MaterialDefinition::s_textureNames[] = {
    "DiffuseMap"_sh,
    "NormalMap"_sh,
    "ParallaxMap"_sh,
    "MetalnessMap"_sh,
    "RoughnessMap"_sh,
    "AoMap"_sh
};

MaterialDefinition::MaterialDefinition()
    : m_attributes {
          .shaderName = s_defaultDefinitionShaderName,
          .bucket = RenderBucket::Opaque,
          .fillMode = FM_FILL,
          .blendFunction = BlendFunction::None(),
          .cullFaces = FCM_BACK,
          .flags = MAF_DEPTH_WRITE | MAF_DEPTH_TEST
      }
{
}

MaterialDefinition::MaterialDefinition(Name name, RenderBucket rb)
    : AssetObject(name),
      m_attributes {
          .shaderName = s_defaultDefinitionShaderName,
          .bucket = rb
      }
{
}

MaterialDefinition::MaterialDefinition(Name name, const MaterialAttributes& attributes)
    : MaterialDefinition(name, attributes, MaterialParameters {}, MaterialTextures {})
{
}

MaterialDefinition::MaterialDefinition(
    Name name,
    const MaterialAttributes& attributes,
    const MaterialParameters& defaultParameters,
    const MaterialTextures& defaultTextures)
    : AssetObject(name),
      m_attributes(attributes),
      m_defaultParameters(defaultParameters),
      m_defaultTextures(defaultTextures)
{
    if (!m_attributes.shaderName)
    {
        m_attributes.shaderName = s_defaultDefinitionShaderName;
    }
}

MaterialDefinition::~MaterialDefinition()
{
    SetReady(false);

    for (size_t i = 0; i < m_defaultTextures.Size(); i++)
    {
        Handle<Texture>& texture = m_defaultTextures.AtIndex(i);

        if (texture)
        {
            EnqueueDeletion(std::move(texture));
        }
    }
}

void MaterialDefinition::Init()
{
    HYP_SCOPE;

    for (size_t i = 0; i < m_defaultTextures.Size(); i++)
    {
        Pair<MaterialTextureKey, Handle<Texture>&> keyValue = m_defaultTextures.KeyValueAt(i);

        const Handle<Texture>& texture = keyValue.second;

        if (!texture.IsValid())
        {
            continue;
        }

        CheckResult(keyValue.second->Create());
    }

    AssetObject::Init();

    SetReady(true);
}

Handle<MaterialInstance> MaterialDefinition::CreateInstance() const
{
    Handle<MaterialInstance> instance = MakeHandle<MaterialInstance>(
        GetName(),
        MakeStrongRef(const_cast<MaterialDefinition*>(this)),
        m_defaultParameters,
        m_defaultTextures);

    instance->Register("$Memory/MaterialInstances");

    return instance;
}

HashCode MaterialDefinition::GetHashCode() const
{
    return GetMaterialDefinitionHashCode(m_attributes, m_defaultParameters, m_defaultTextures);
}

#pragma endregion MaterialDefinition

#pragma region MaterialInstanceCache

void MaterialInstanceCache::Add(const Handle<MaterialInstance>& instance)
{
    if (!instance)
    {
        return;
    }

    Mutex::Guard guard(m_mutex);

    const HashCode hc = GetMaterialDefinitionHashCode(
        instance->GetAttributes(),
        instance->GetParameters(),
        instance->GetTextures());

    m_map.Set(hc, instance);
}

Handle<MaterialInstance> MaterialInstanceCache::Create(
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
        tmpAttributes.shaderName = s_defaultDefinitionShaderName;

        attributesPtr = &tmpAttributes;
    }

    Handle<MaterialDefinition> definition = MakeHandle<MaterialDefinition>(
        name,
        *attributesPtr,
        parameters,
        textures);

    definition->Register("$Memory/MaterialDefinitions");
    InitObject(definition);

    Handle<MaterialInstance> instance = definition->CreateInstance();
    InitObject(instance);

    return instance;
}

Handle<MaterialInstance> MaterialInstanceCache::GetOrCreate(
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
        tmpAttributes.shaderName = s_defaultDefinitionShaderName;

        attributesPtr = &tmpAttributes;
    }

    const HashCode hc = GetMaterialDefinitionHashCode(*attributesPtr, parameters, textures);

    Handle<MaterialInstance> strongRef;

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
            name = Name::Unique(ANSIString("cached_material_inst_") + ANSIString::ToString(hc.Value()));
        }

        Handle<MaterialDefinition> definition = MakeHandle<MaterialDefinition>(
            name,
            *attributesPtr,
            parameters,
            textures);

        definition->Register("$Memory/MaterialDefinitions");

        strongRef = definition->CreateInstance();

        m_map.Set(hc, strongRef);
    }

    InitObject(strongRef->GetDefinition());
    InitObject(strongRef);

    return strongRef;
}

#pragma endregion MaterialInstanceCache

} // namespace Hyperion
