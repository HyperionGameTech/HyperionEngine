/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetObject.hpp>

#include <rendering/MaterialTypes.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/containers/HashMap.hpp>

#include <Core/threading/Mutex.hpp>

namespace Hyperion {

class MaterialInstance;

HYP_CLASS()
class HYP_API MaterialDefinition final : public AssetObject
{
    HYP_OBJECT_BODY(MaterialDefinition);

public:
    static const StringHash s_textureNames[];

    MaterialDefinition();

    explicit MaterialDefinition(
        Name name,
        RenderBucket rb = RenderBucket::Opaque);

    MaterialDefinition(
        Name name,
        const MaterialAttributes& attributes);

    MaterialDefinition(
        Name name,
        const MaterialAttributes& attributes,
        const MaterialParameters& defaultParameters,
        const MaterialTextures& defaultTextures);

    MaterialDefinition(const MaterialDefinition& other) = delete;
    MaterialDefinition& operator=(const MaterialDefinition& other) = delete;

    MaterialDefinition(MaterialDefinition&& other) noexcept = delete;
    MaterialDefinition& operator=(MaterialDefinition&& other) noexcept = delete;

    ~MaterialDefinition() override;

    HYP_FORCE_INLINE const MaterialAttributes& GetAttributes() const
    {
        return m_attributes;
    }

    HYP_FORCE_INLINE MaterialAttributes& GetAttributes()
    {
        return m_attributes;
    }

    HYP_FORCE_INLINE RenderBucket GetBucket() const
    {
        return m_attributes.bucket;
    }

    HYP_FORCE_INLINE const MaterialParameters& GetDefaultParameters() const
    {
        return m_defaultParameters;
    }

    HYP_FORCE_INLINE const MaterialTextures& GetDefaultTextures() const
    {
        return m_defaultTextures;
    }

    /*! \brief Create a new MaterialInstance from this definition */
    Handle<MaterialInstance> CreateInstance() const;

    HashCode GetHashCode() const;

private:
    void Init() override;

    HYP_FIELD(Property = "Attributes", Serialize, Editor)
    MaterialAttributes m_attributes;

    HYP_FIELD(Property = "DefaultParameters", Editor, Serialize)
    MaterialParameters m_defaultParameters;

    HYP_FIELD(Property = "DefaultTextures", Editor, Serialize)
    MaterialTextures m_defaultTextures;
};

class MaterialInstanceCache
{
public:
    void Add(const Handle<MaterialInstance>& instance);

    Handle<MaterialInstance> Create(
        Name name,
        const MaterialAttributes& attributes = {},
        const MaterialParameters& parameters = {},
        const MaterialTextures& textures = {});

    HYP_FORCE_INLINE Handle<MaterialInstance> Create(
        const MaterialAttributes& attributes = {},
        const MaterialParameters& parameters = {},
        const MaterialTextures& textures = {})
    {
        return Create(Name::Unique("MaterialInstance"), attributes, parameters, textures);
    }

    Handle<MaterialInstance> GetOrCreate(
        Name name,
        const MaterialAttributes& attributes = {},
        const MaterialParameters& parameters = {},
        const MaterialTextures& textures = {});

    HYP_FORCE_INLINE Handle<MaterialInstance> GetOrCreate(
        const MaterialAttributes& attributes = {},
        const MaterialParameters& parameters = {},
        const MaterialTextures& textures = {})
    {
        return GetOrCreate(Name::Invalid(), attributes, parameters, textures);
    }

private:
    HashMap<HashCode, WeakHandle<MaterialInstance>> m_map;
    Mutex m_mutex;
};

} // namespace Hyperion
