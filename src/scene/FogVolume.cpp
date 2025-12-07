/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include "core/reflection/Handle.hpp"
#include <HyperionPch.hpp>

#include <scene/FogVolume.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Shared.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/threading/Threads.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/MathUtil.hpp>

#include <engine/EngineGlobals.hpp>

#include <FogVolume.generated.inl>

namespace hyperion {

FogVolume::FogVolume()
{
}

FogVolume::FogVolume(const BoundingBox& aabb)
{
    m_localBounds = aabb;
}

FogVolume::~FogVolume()
{
    if (m_volumeTexture)
    {
        SafeDelete(std::move(m_volumeTexture));
    }
}

void FogVolume::Init()
{
    Entity::Init();

    if (m_volumeTexture)
    {
        InitObject(m_volumeTexture);
    }

    SetReady(true);
}

void FogVolume::SetVolumeTexture(const Handle<Texture>& texture)
{
    if (m_volumeTexture == texture)
    {
        return;
    }

    if (m_volumeTexture)
    {
        SafeDelete(std::move(m_volumeTexture));
    }

    m_volumeTexture = texture;

    if (IsInitCalled())
    {
        InitObject(m_volumeTexture);

        SetNeedsRenderProxyUpdate();
    }
}

void FogVolume::UpdateRenderProxy(RenderProxyFogVolume* proxy)
{
    AssertDebug(proxy != nullptr);

    proxy->fogVolume = WeakHandleFromThis();

    if (proxy->volumeTexture != m_volumeTexture)
    {
        proxy->forceRebind = true;

        proxy->volumeTexture = m_volumeTexture;
    }

    proxy->worldAabb = GetWorldAABB();
}

} // namespace hyperion
