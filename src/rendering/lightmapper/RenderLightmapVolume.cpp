/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/RenderLightmapVolume.hpp>

#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

#pragma region RenderLightmapVolume

RenderLightmapVolume::RenderLightmapVolume(LightmapVolume* lightmap_volume)
    : m_lightmap_volume(lightmap_volume)
{
}

RenderLightmapVolume::~RenderLightmapVolume() = default;

void RenderLightmapVolume::Initialize_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_lightmap_volume != nullptr);
}

void RenderLightmapVolume::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderLightmapVolume::Update_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_lightmap_volume != nullptr);
}

#pragma endregion RenderLightmapVolume

} // namespace hyperion