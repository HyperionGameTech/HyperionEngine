/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <rendering/shadows/ShadowMap.hpp>
#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/Texture.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region ShadowMap

ShadowMap::ShadowMap(ShadowMapType type, ShadowMapFilter filterMode, const ShadowMapAtlasElement& atlasElement, const GpuImageViewRef& imageView)
    : m_type(type),
      m_filterMode(filterMode),
      m_atlasElement(new ShadowMapAtlasElement(atlasElement)),
      m_imageView(imageView)
{
}

ShadowMap::~ShadowMap()
{
    SafeRelease(std::move(m_imageView));

    if (m_atlasElement)
    {
        delete m_atlasElement;
        m_atlasElement = nullptr;
    }
}

#pragma endregion ShadowMap

} // namespace hyperion
