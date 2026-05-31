/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/shadows/ShadowMap.hpp>
#include <Rendering/shadows/ShadowMapAllocator.hpp>

#include <Rendering/Buffers.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/FullScreenPass.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Texture.hpp>

#include <Rendering/util/DeletionQueue.hpp>

#include <Scene/Light.hpp>
#include <Scene/View.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Core/logging/Logger.hpp>

#include <Framework/EngineGlobals.hpp>

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
