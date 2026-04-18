/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetObject.hpp>

#include <rendering/MaterialTypes.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/containers/String.hpp>
#include <Core/containers/HashMap.hpp>

namespace Hyperion {

class MaterialDefinition;
class RenderProxyMaterial;

HYP_CLASS(AssetBucket = "MaterialInstances")
class HYP_API MaterialInstance final : public AssetObject
{
    HYP_OBJECT_BODY(MaterialInstance);

public:
    MaterialInstance();

    explicit MaterialInstance(
        Handle<MaterialDefinition> definition);

    MaterialInstance(
        Name name,
        Handle<MaterialDefinition> definition);

    MaterialInstance(
        Name name,
        Handle<MaterialDefinition> definition,
        const MaterialParameters& parameters,
        const MaterialTextures& textures);

    MaterialInstance(const MaterialInstance& other) = delete;
    MaterialInstance& operator=(const MaterialInstance& other) = delete;

    MaterialInstance(MaterialInstance&& other) noexcept = delete;
    MaterialInstance& operator=(MaterialInstance&& other) noexcept = delete;

    ~MaterialInstance() override;

    HYP_FORCE_INLINE const Handle<MaterialDefinition>& GetDefinition() const
    {
        return m_definition;
    }

    /*! \brief Get the render attributes for this MaterialInstance, delegated from the definition.
     *  \return The MaterialAttributes owned by this instance's MaterialDefinition. */
    const MaterialAttributes& GetAttributes() const;

    HYP_FORCE_INLINE const MaterialParameters& GetParameters() const
    {
        return m_parameters;
    }

    void SetParameters(const MaterialParameters& parameters);

    /*! \brief Reset all parameters back to the definition's defaults. */
    void ResetParameters();

    /*! \brief Set a texture slot. If the instance is already initialized, the texture
     *  will be initialized immediately. \param key The slot to set. \param texture The texture handle to set. */
    void SetTexture(MaterialTextureKey key, const Handle<Texture>& texture);

    /*! \brief Set a texture by slot index. \param index The index to set. \param texture The texture handle to set. */
    void SetTextureAtIndex(uint32 index, const Handle<Texture>& texture);

    /*! \brief Replace all textures. \param textures The new texture set. */
    HYP_METHOD(Property = "Textures", NoScriptBindings)
    void SetTextures(const MaterialTextures& textures);

    HYP_METHOD(Property = "Textures", NoScriptBindings)
    HYP_FORCE_INLINE const MaterialTextures& GetTextures() const
    {
        return m_textures;
    }

    const Handle<Texture>& GetTexture(MaterialTextureKey key) const;
    const Handle<Texture>& GetTextureAtIndex(uint32 index) const;

    /*! \brief Get the render bucket, delegated from the definition. */
    HYP_FORCE_INLINE RenderBucket GetBucket() const;

    /*! \brief Returns true if the instance is static (not expected to change frequently)
     *  and may be shared across many objects. */
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

    /*! \brief Enqueue a render proxy update to push current state to the GPU. */
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

    /*! \brief Clone this instance. The clone shares the same MaterialDefinition but
     *  gets its own copy of the parameters and textures. The clone is dynamic by default. */
    HYP_METHOD(NotNullReturn)
    Handle<MaterialInstance> Clone() const;

    HashCode GetHashCode() const;

private:
    void Init() override;

    HYP_FIELD(Property = "Definition", Serialize)
    Handle<MaterialDefinition> m_definition;

    HYP_FIELD(Property = "Parameters", Serialize, Editor)
    MaterialParameters m_parameters;

    HYP_FIELD(Property = "Textures", Serialize, Editor)
    MaterialTextures m_textures;

    HYP_FIELD()
    bool m_isDynamic;

    int m_renderProxyVersion;
};

} // namespace Hyperion
