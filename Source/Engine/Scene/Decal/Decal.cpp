/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Decal/Decal.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/Util/DeletionQueue.hpp>

#include <Decal.generated.inl>

namespace Hyperion {

Decal::Decal()
    : m_renderProxyVersion(0)
{
}

Decal::Decal(Name name, const DecalDesc& decalDesc)
    : AssetObject(name),
      m_decalDesc(decalDesc),
      m_renderProxyVersion(0)
{
}

Decal::~Decal()
{
    if (m_decalDesc.material.IsValid())
    {
        EnqueueDeletion(std::move(m_decalDesc.material));
    }
}

void Decal::SetDecalDesc(const DecalDesc& decalDesc)
{
    if (m_decalDesc.material.IsValid()
        && m_decalDesc.material != decalDesc.material)
    {
        EnqueueDeletion(std::move(m_decalDesc.material));
    }

    m_decalDesc = decalDesc;

    SetNeedsRenderProxyUpdate();

    MarkDirty();
}

} // namespace Hyperion
