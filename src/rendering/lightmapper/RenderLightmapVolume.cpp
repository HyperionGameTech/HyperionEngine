/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/RenderLightmapVolume.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/rhi/RHICommandList.hpp>

#include <scene/lightmapper/LightmapVolume.hpp>

#include <scene/Texture.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

// temp
#include <util/img/Bitmap.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DESCRIPTOR_SSBO(Global, LightmapVolumesBuffer, 1, ~0u, false);

} // namespace hyperion