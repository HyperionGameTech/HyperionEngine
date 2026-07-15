/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Rendering/MaterialTypes.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Threading/Mutex.hpp>

namespace Hyperion {

struct RenderProxyMaterial;

HYP_CLASS(AssetBucket = "Materials")
class ENGINE_API Material final : public AssetObject
{
    HYP_OBJECT_BODY(Material);

public:
    static const StringHash s_textureNames[];

    Material();

    explicit Material(
        Name name,
        RenderBucket rb = RenderBucket::Opaque);

    Material(
        Name name,
        const MaterialAttributes& attributes);

    Material(
        Name name,
        const Handle<Material>& base);

    Material(
        Name name,
        const MaterialAttributes& attributes,
        const MaterialParameters& parameters,
        const MaterialTextures& textures);

    Material(
        Name name,
        const Handle<Material>& base,
        const MaterialAttributes& attributes,
        const MaterialParameters& parameters,
        const MaterialTextures& textures);

    Material(const Material& other) = delete;
    Material& operator=(const Material& other) = delete;

    Material(Material&& other) noexcept = delete;
    Material& operator=(Material&& other) noexcept = delete;

    ~Material() override;

    HYP_METHOD(Property = "BaseMaterial", Serialize, Editor)
    HYP_FORCE_INLINE const Handle<Material>& GetBaseMaterial() const
    {
        return m_base;
    }

    HYP_METHOD(Property = "BaseMaterial", Serialize, Editor)
    void SetBaseMaterial(const Handle<Material>& baseMaterial);

    const MaterialAttributes& GetAttributes() const;
    MaterialAttributes& GetAttributes();

    HYP_FORCE_INLINE RenderBucket GetBucket() const
    {
        return GetAttributes().bucket;
    }

    HYP_FORCE_INLINE const MaterialParameters& GetParameters() const
    {
        return m_parameters;
    }

    void SetParameters(const MaterialParameters& parameters);

    void ResetParameters();

    HYP_METHOD(Property = "Textures", NoScriptBindings)
    void SetTextures(const MaterialTextures& textures);

    HYP_METHOD(Property = "Textures", NoScriptBindings)
    HYP_FORCE_INLINE const MaterialTextures& GetTextures() const
    {
        return m_textures;
    }

    void SetTexture(MaterialTextureKey key, const Handle<Texture>& texture);
    void SetTextureAtIndex(uint32 index, const Handle<Texture>& texture);
    const Handle<Texture>& GetTexture(MaterialTextureKey key) const;
    const Handle<Texture>& GetTextureAtIndex(uint32 index) const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsStatic() const
    {
        return !m_isDynamic;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool GetIsDynamic() const
    {
        return m_isDynamic;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetIsDynamic(bool isDynamic)
    {
        m_isDynamic = isDynamic;
    }

    const int* GetRenderProxyVersionPtr() const
    {
        return &m_renderProxyVersion;
    }

    void SetNeedsRenderProxyUpdate()
    {
        ++m_renderProxyVersion;
    }

    void UpdateRenderProxy(RenderProxyMaterial* proxy);

    HYP_METHOD(NotNullReturn)
    Handle<Material> Clone() const;

    HashCode GetHashCode() const;

private:
    void Init() override;

    HYP_FIELD(Property = "BaseMaterial", Serialize, Editor)
    Handle<Material> m_base;

    HYP_FIELD(Property = "Attributes", Serialize, Editor)
    MaterialAttributes m_attributes;

    HYP_FIELD(Property = "Parameters", Serialize, Editor)
    MaterialParameters m_parameters;

    HYP_FIELD(Property = "Textures", Serialize, Editor)
    MaterialTextures m_textures;

    HYP_FIELD()
    bool m_isDynamic;

    int m_renderProxyVersion;
};

class MaterialCache
{
public:
    void Add(const Handle<Material>& material);

    Handle<Material> Create(
        Name name,
        const MaterialAttributes& attributes = {},
        const MaterialParameters& parameters = {},
        const MaterialTextures& textures = {});

    HYP_FORCE_INLINE Handle<Material> Create(
        const MaterialAttributes& attributes = {},
        const MaterialParameters& parameters = {},
        const MaterialTextures& textures = {})
    {
        return Create(Name::Unique("Material"), attributes, parameters, textures);
    }

    Handle<Material> GetOrCreate(
        Name name,
        const MaterialAttributes& attributes = {},
        const MaterialParameters& parameters = {},
        const MaterialTextures& textures = {});

    HYP_FORCE_INLINE Handle<Material> GetOrCreate(
        const MaterialAttributes& attributes = {},
        const MaterialParameters& parameters = {},
        const MaterialTextures& textures = {})
    {
        return GetOrCreate(Name::Invalid(), attributes, parameters, textures);
    }

private:
    Map<HashCode, WeakHandle<Material>> m_map;
    SharedMutex m_mutex;
};

} // namespace Hyperion
