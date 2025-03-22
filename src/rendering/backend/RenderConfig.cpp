
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <Engine.hpp>

namespace hyperion {
namespace renderer {

bool RenderConfig::ShouldCollectUniqueDrawCallPerMaterial()
{
    return true;//!IsBindlessSupported();
}

bool RenderConfig::IsBindlessSupported()
{
    return g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures();
}

bool RenderConfig::IsRaytracingSupported()
{
    return g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported();
}

bool RenderConfig::IsIndirectRenderingEnabled()
{
    return true;
}

bool RenderConfig::IsParallelRenderingEnabled()
{
    return true;
}

} // namespace renderer
} // namespace hyperion