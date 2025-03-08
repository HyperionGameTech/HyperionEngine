/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

LightmapVolume::LightmapVolume()
    : m_aabb(BoundingBox::Empty())
{
}
    
LightmapVolume::LightmapVolume(const BoundingBox &aabb)
    : m_aabb(aabb)
{
}

LightmapVolume::~LightmapVolume()
{
}

void LightmapVolume::SetAABB(const BoundingBox &aabb)
{
    HYP_NOT_IMPLEMENTED();
}

void LightmapVolume::Init()
{
    if (IsInitCalled()) {
        return;
    }
    
    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
    {
    }));

    SetReady(true);
}

} // namespace hyperion
