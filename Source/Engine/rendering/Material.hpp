/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <rendering/RenderableAttributes.hpp>

#include <Core/containers/FixedArray.hpp>
#include <Core/containers/String.hpp>
#include <Core/containers/HashMap.hpp>

#include <Core/threading/Mutex.hpp>

#include <Core/reflection/ObjectFwd.hpp>

#include <Core/math/Color.hpp>

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <asset/AssetObject.hpp>

#include <util/EnumOptions.hpp>

namespace Hyperion {

class Texture;
class RenderProxyMaterial;

HYP_ENUM()
enum class MaterialTextureKey : uint64
{
    NONE = 0,

    Diffuse = 0x1,
    Normals = 0x2,
    Parallax = 0x4,
    Metalness = 0x8,
    Roughness = 0x10,
    AmbientOcclusion = 0x20
};

#pragma pack(push, 1)

HYP_STRUCT()
class MaterialParameters
{
public:
    HYP_STRUCT_BODY(MaterialParameters);

    HYP_FIELD(Property = "Albedo", Editor, Serialize)
    Vec4f albedo;

    HYP_FIELD(Property = "Metalness", Editor, Serialize)
    float metalness;

    HYP_FIELD(Property = "Roughness", Editor, Serialize)
    float roughness;

    HYP_FIELD(Property = "AlphaThreshold", Editor, Serialize)
    float alphaThreshold;

    HYP_FIELD(Property = "ParallaxHeightScale", Editor, Serialize)
    float parallaxHeightScale;

    HYP_FIELD(Property = "Transmission", Editor, Serialize)
    float transmission;

    HYP_FIELD(Property = "IOR", Editor, Serialize)
    float ior;

    HYP_FIELD(Property = "EmissiveColor", Editor, Serialize)
    Color emissiveColor;

    HYP_FIELD(Property = "EmissiveIntensity", Editor, Serialize)
    float emissiveIntensity;

    HYP_FIELD(Property = "UserParams", Serialize)
    Vec4f userParams;

    MaterialParameters()
        : albedo(1.0f),
          metalness(0.0f),
          roughness(1.0f),
          alphaThreshold(0.0f),
          parallaxHeightScale(0.02f),
          transmission(0.0f),
          ior(1.5f),
          emissiveColor(),
          emissiveIntensity(0.0f),
          userParams(0.0f)
    {
    }

    MaterialParameters(const MaterialParameters& other) = default;
    MaterialParameters& operator=(const MaterialParameters& other) = default;

    MaterialParameters(MaterialParameters&& other) noexcept = default;
    MaterialParameters& operator=(MaterialParameters&& other) noexcept = default;

    HYP_FORCE_INLINE bool operator==(const MaterialParameters& other) const
    {
        return memcmp(this, &other, sizeof(MaterialParameters)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const MaterialParameters& other) const
    {
        return memcmp(this, &other, sizeof(MaterialParameters)) != 0;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode((const ubyte*)this, (const ubyte*)this + sizeof(MaterialParameters));
    }
};

#pragma pack(pop)

HYP_STRUCT()
class MaterialTextures
{
public:
    HYP_STRUCT_BODY(MaterialTextures);

    static constexpr uint32 MaxTextures = 32u;

    using Iterator = FixedArray<Handle<Texture>, MaxTextures>::Iterator;
    using ConstIterator = FixedArray<Handle<Texture>, MaxTextures>::ConstIterator;

    MaterialTextures() = default;

    MaterialTextures(std::initializer_list<Pair<MaterialTextureKey, Handle<Texture>>> initializerList)
    {
        for (const auto& it : initializerList)
        {
            m_values[EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(it.first)] = it.second;
        }
    }

    MaterialTextures(const MaterialTextures& other) = default;
    MaterialTextures& operator=(const MaterialTextures& other) = default;

    MaterialTextures(MaterialTextures&& other) noexcept = default;
    MaterialTextures& operator=(MaterialTextures&& other) noexcept = default;

    ~MaterialTextures() = default;

    HYP_FORCE_INLINE bool operator==(const MaterialTextures& other) const
    {
        return m_values == other.m_values;
    }

    HYP_FORCE_INLINE bool operator!=(const MaterialTextures& other) const
    {
        return m_values != other.m_values;
    }

    HYP_FORCE_INLINE Handle<Texture>& operator[](MaterialTextureKey key)
    {
        return m_values[EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(key)];
    }

    HYP_FORCE_INLINE const Handle<Texture>& operator[](MaterialTextureKey key) const
    {
        return m_values[EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(key)];
    }

    HYP_FORCE_INLINE bool Has(MaterialTextureKey key) const
    {
        return bool(m_values[EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(key)]);
    }

    HYP_FORCE_INLINE Handle<Texture>& AtIndex(size_t index)
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE const Handle<Texture>& AtIndex(size_t index) const
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE Pair<MaterialTextureKey, Handle<Texture>&> KeyValueAt(size_t index)
    {
        return Pair<MaterialTextureKey, Handle<Texture>&>(
            EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::OrdinalToEnum(index),
            m_values[index]);
    }

    HYP_FORCE_INLINE Pair<MaterialTextureKey, const Handle<Texture>&> KeyValueAt(size_t index) const
    {
        return Pair<MaterialTextureKey, const Handle<Texture>&>(
            EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::OrdinalToEnum(index),
            m_values[index]);
    }

    HYP_FORCE_INLINE size_t Size() const
    {
        return m_values.Size();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return m_values.GetHashCode();
    }

    HYP_DEF_STL_BEGIN_END(m_values.Begin(), m_values.End());

private:
    HYP_FIELD(Serialize)
    FixedArray<Handle<Texture>, MaxTextures> m_values;
};

HYP_CLASS()
class HYP_API Material final : public AssetObject
{
    HYP_OBJECT_BODY(Material);

public:
    static const StringHash s_textureNames[];

    enum State
    {
        MATERIAL_STATE_CLEAN,
        MATERIAL_STATE_DIRTY
    };

    Material();

    explicit Material(
        Name name,
        RenderBucket rb = RenderBucket::Opaque);

    Material(
        Name name,
        const MaterialAttributes& attributes);

    Material(
        Name name,
        const MaterialAttributes& attributes,
        const MaterialParameters& parameters,
        const MaterialTextures& textures);

    Material(const Material& other) = delete;
    Material& operator=(const Material& other) = delete;

    Material(Material&& other) noexcept = delete;
    Material& operator=(Material&& other) noexcept = delete;

    ~Material() override;

    HYP_FORCE_INLINE const MaterialParameters& GetParameters() const
    {
        return m_parameters;
    }

    void SetParameters(const MaterialParameters& parameters);

    /*! \brief Set all parameters back to their default values. */
    void ResetParameters();

    /*! \brief Sets the texture with the given key on this Material.
     *  If the Material has already been initialized, the Texture is initialized.
     *  Otherwise, it will be initialized when the Material is initialized.
     *  \param key The texture slot to set the texture on
     *  \param texture The Texture handle to set. */
    void SetTexture(MaterialTextureKey key, const Handle<Texture>& texture);

    /*! \brief Sets the texture at the given index on this Material.
     *  If the Material has already been initialized, the Texture is initialized.
     *  Otherwise, it will be initialized when the Material is initialized.
     *  \param index The index to set the texture in
     *  \param texture The Texture handle to set. */
    void SetTextureAtIndex(uint32 index, const Handle<Texture>& texture);

    /*! \brief Sets all textures on this Material to the given MaterialTextures.
     *  If the Material has already been initialized, the Textures are initialized.
     *  Otherwise, they will be initialized when the Material is initialized.
     *  \param textures The textures to set on this Material. */
    HYP_METHOD(Property = "Textures", NoScriptBindings)
    void SetTextures(const MaterialTextures& textures);

    HYP_METHOD(Property = "Textures", NoScriptBindings)
    HYP_FORCE_INLINE const MaterialTextures& GetTextures() const
    {
        return m_textures;
    }

    /*! \brief Return a pointer to a Texture set on this Material by the given
     *  texture key. If no Texture was set, nullptr is returned.
     *  \param key The key of the texture to find
     *  \return Handle for the found Texture, or an empty Handle if not found. */
    const Handle<Texture>& GetTexture(MaterialTextureKey key) const;

    /*! \brief Return a pointer to a Texture set on this Material by the given
     *  index. If no Texture was set, nullptr is returned.
     *  \param index The index of the texture to find
     *  \return Handle for the found Texture, or an empty Handle if not found. */
    const Handle<Texture>& GetTextureAtIndex(uint32 index) const;

    /*! \brief Get the bucket for this Material.
     *  \return The bucket for this Material. */
    HYP_FORCE_INLINE RenderBucket GetBucket() const
    {
        return m_attributes.bucket;
    }

    /*! \brief Set the bucket for this Material.
     *  \param rb The bucket to set. */
    HYP_FORCE_INLINE void SetBucket(RenderBucket rb)
    {
        m_attributes.bucket = rb;
    }

    /*! \brief Get whether this Material is alpha blended.
     *  \return True if the Material is alpha blended, false otherwise. */
    HYP_FORCE_INLINE bool IsAlphaBlended() const
    {
        return m_attributes.blendFunction != BlendFunction::None();
    }

    /*! \brief Set whether this Material is alpha blended.
     *  \param isAlphaBlended True if the Material is alpha blended, false otherwise.
     *  \param blendFunction The blend function to use if the Material is alpha blended. By default, it is set to \ref{BlendFunction::AlphaBlending()}. */
    HYP_FORCE_INLINE void SetIsAlphaBlended(bool isAlphaBlended, BlendFunction blendFunction = BlendFunction::AlphaBlending())
    {
        if (isAlphaBlended)
        {
            m_attributes.blendFunction = blendFunction;
        }
        else
        {
            m_attributes.blendFunction = BlendFunction::None();
        }
    }

    /*! \brief Get the blend function for this Material.
     *  \return The blend function for this Material. */
    HYP_FORCE_INLINE BlendFunction GetBlendFunction() const
    {
        return m_attributes.blendFunction;
    }

    /*! \brief Set the blend function for this Material.
     *  \param blendFunction The blend function to set. */
    HYP_FORCE_INLINE void SetBlendMode(BlendFunction blendFunction)
    {
        m_attributes.blendFunction = blendFunction;
    }

    /*! \brief Get whether depth writing is enabled for this Material.
     *  \return True if depth writing is enabled, false otherwise. */
    HYP_FORCE_INLINE bool IsDepthWriteEnabled() const
    {
        return bool(m_attributes.flags & MAF_DEPTH_WRITE);
    }

    /*! \brief Set whether depth writing is enabled for this Material.
     *  \param isDepthWriteEnabled True if depth writing is enabled, false otherwise. */
    HYP_FORCE_INLINE void SetIsDepthWriteEnabled(bool isDepthWriteEnabled)
    {
        if (isDepthWriteEnabled)
        {
            m_attributes.flags |= MAF_DEPTH_WRITE;
        }
        else
        {
            m_attributes.flags &= ~MAF_DEPTH_WRITE;
        }
    }

    /*! \brief Get whether depth testing is enabled for this Material.
     *  \return True if depth testing is enabled, false otherwise. */
    HYP_FORCE_INLINE bool IsDepthTestEnabled() const
    {
        return bool(m_attributes.flags & MAF_DEPTH_TEST);
    }

    /*! \brief Set whether depth testing is enabled for this Material.
     *  \param isDepthTestEnabled True if depth testing is enabled, false otherwise. */
    HYP_FORCE_INLINE void SetIsDepthTestEnabled(bool isDepthTestEnabled)
    {
        if (isDepthTestEnabled)
        {
            m_attributes.flags |= MAF_DEPTH_TEST;
        }
        else
        {
            m_attributes.flags &= ~MAF_DEPTH_TEST;
        }
    }

    /*! \brief Get the face culling mode for this Material.
     *  \return The face culling mode for this Material. */
    HYP_FORCE_INLINE FaceCullMode GetFaceCullMode() const
    {
        return m_attributes.cullFaces;
    }

    /*! \brief Set the face culling mode for this Material.
     *  \param cullMode The face culling mode to set. */
    HYP_FORCE_INLINE void SetFaceCullMode(FaceCullMode cullMode)
    {
        m_attributes.cullFaces = cullMode;
    }

    /*! \brief Get the render attributes of this Material.
     *  \return The render attributes of this Material. */
    HYP_FORCE_INLINE MaterialAttributes& GetAttributes()
    {
        return m_attributes;
    }

    /*! \brief Get the render attributes of this Material.
     *  \return The render attributes of this Material. */
    HYP_FORCE_INLINE const MaterialAttributes& GetAttributes() const
    {
        return m_attributes;
    }

    /*! \brief If a Material is static, it is expected to not change frequently and
     *  may be shared across many objects. Otherwise, it is considered dynamic and may
     *  be modified.
     *  \return True if the Material is static, false if it is dynamic. */
    HYP_METHOD()
    HYP_FORCE_INLINE bool IsStatic() const
    {
        return !m_isDynamic;
    }

    /*! \brief If a Material is
     *  dynamic, it is expected to change frequently and may be modified. Otherwise,
     *  it is considered static and should not be modified as it may be shared across many
     *  objects.
     *  \return True if the Material is dynamic, false if it is static. */
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

    /*! \brief If the Material's mutation state is dirty, this will
     * create a task on the render thread to update the Material's
     * data on the GPU. */
    void EnqueueRenderUpdates();

    const int* GetRenderProxyVersionPtr() const
    {
        return &m_renderProxyVersion;
    }

    void SetNeedsRenderProxyUpdate()
    {
        ++m_renderProxyVersion;
    }

    void UpdateRenderProxy(RenderProxyMaterial* proxy);

    /*! \brief Clone this Material. The cloned Material will have the same
     *  shader, parameters, textures, and render attributes as the original.
     *  \details Using this method is a good way to get around the fact that
     *  static Materials are shared across many objects. If you need to modify
     *  a static Material, clone it first. The cloned Material will be dynamic
     *  by default, and can be modified without affecting the original Material.
     *  \note The cloned Material will not be initialized.
     *  \return A new Material that is a clone of this Material. */
    HYP_METHOD(NotNullReturn)
    Handle<Material> Clone() const;

    HashCode GetHashCode() const;

private:
    void Init() override;

    // set the texture mask in MaterialAttributes based on currently set textures
    void UpdateAttributesTextureMask();

    HYP_FIELD(Property = "Parameters", Serialize, Editor)
    MaterialParameters m_parameters;

    HYP_FIELD(Property = "Textures", Serialize, Editor)
    MaterialTextures m_textures;

    HYP_FIELD(Property = "Attributes", Serialize, Editor)
    MaterialAttributes m_attributes;

    HYP_FIELD()
    bool m_isDynamic;

    int m_renderProxyVersion;
};

HYP_CLASS()
class MaterialGroup final : public ObjectBase
{
    HYP_OBJECT_BODY(MaterialGroup);

public:
    MaterialGroup();
    MaterialGroup(const MaterialGroup& other) = delete;
    MaterialGroup& operator=(const MaterialGroup& other) = delete;
    ~MaterialGroup() override;

    void Add(const String& name, Handle<Material>&& material);
    bool Remove(const String& name);

    Handle<Material>& Get(const String& name)
    {
        return m_materials[name];
    }

    const Handle<Material>& Get(const String& name) const
    {
        return m_materials.At(name);
    }

    bool Has(const String& name) const
    {
        return m_materials.Contains(name);
    }

private:
    void Init() override;

    HashMap<String, Handle<Material>> m_materials;
};

class MaterialCache
{
public:
    static MaterialCache* GetInstance();

    void Add(const Handle<Material>& material);

    Handle<Material> CreateMaterial(
        Name name,
        const MaterialAttributes& attributes = {},
        const MaterialParameters& parameters = {},
        const MaterialTextures& textures = {});

    HYP_FORCE_INLINE Handle<Material> CreateMaterial(
        const MaterialAttributes& attributes = {},
        const MaterialParameters& parameters = {},
        const MaterialTextures& textures = {})
    {
        return CreateMaterial(Name::Unique("Material"), attributes, parameters, textures);
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
    HashMap<HashCode, WeakHandle<Material>> m_map;
    Mutex m_mutex;
};

} // namespace Hyperion
