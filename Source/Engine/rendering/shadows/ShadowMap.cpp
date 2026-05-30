/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/shadows/ShadowMap.hpp>
#include <rendering/shadows/ShadowMapAllocator.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Texture.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>

#include <ShadowMap.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region ShadowMap

ShadowMap::ShadowMap(
    ShadowMapType type,
    ShadowMapFilter filterMode,
    const ShadowMapAtlasElement& atlasElement,
    const GpuImageViewRef& imageView)
    : m_type(type),
      m_filterMode(filterMode),
      m_atlasElement(new ShadowMapAtlasElement(atlasElement)),
      m_imageView(imageView)
{
}

ShadowMap::~ShadowMap()
{
    EnqueueDeletion(std::move(m_imageView));

    if (m_atlasElement)
    {
        delete m_atlasElement;
        m_atlasElement = nullptr;
    }
}

#pragma endregion ShadowMap

} // namespace Hyperion
