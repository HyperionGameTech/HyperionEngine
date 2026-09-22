/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Math/Vector3.hpp>

#include <Scene/Decal/DecalTypes.hpp>

namespace Hyperion {

class Material;

/// Decal type asset.
HYP_CLASS(AssetBucket = "Decals")
class ENGINE_API Decal : public AssetObject
{
    HYP_OBJECT_BODY(Decal);

public:
    Decal();
    Decal(Name name, const DecalDesc& decalDesc);

    ~Decal() override;

    HYP_METHOD(Property = "Desc", Serialize, Editor, NoScriptBindings)
    HYP_FORCE_INLINE const DecalDesc& GetDecalDesc() const
    {
        return m_decalDesc;
    }

    HYP_METHOD(Property = "Desc", Serialize, Editor, NoScriptBindings)
    void SetDecalDesc(const DecalDesc& decalDesc);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Material>& GetMaterial() const
    {
        return m_decalDesc.material;
    }

    const int* GetRenderProxyVersionPtr() const
    {
        return &m_renderProxyVersion;
    }

    void SetNeedsRenderProxyUpdate()
    {
        ++m_renderProxyVersion;
    }

private:
    DecalDesc m_decalDesc;
    int m_renderProxyVersion;
};

} // namespace Hyperion
