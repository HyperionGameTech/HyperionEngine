/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/FogVolume.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Shared.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/reflection/Handle.hpp>
#include <core/threading/Threads.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/MathUtil.hpp>

#include <engine/EngineGlobals.hpp>

#include <FogVolume.generated.inl>

namespace hyperion {

FogVolume::FogVolume()
{
}

FogVolume::FogVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds)
{
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
    VolumeBase::Init();

    if (m_volumeTexture)
    {
        InitObject(m_volumeTexture);
    }

    SetNeedsRenderProxyUpdate();

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

    const BoundingBox worldAabb = GetWorldAABB();

    proxy->fogVolume = WeakHandleFromThis();
    proxy->worldAabb = worldAabb;

    if (proxy->volumeTexture != m_volumeTexture)
    {
        proxy->forceRebind = true;

        proxy->volumeTexture = m_volumeTexture;
    }

    // create transform matrix turning 1:1:1 cube to the world bounds
    const Vec3f boxSize = m_localBounds.GetExtent();

    const Mat4f newTransformMatrix = GetWorldTransform().GetMatrix() * Mat4f::Scaling(boxSize);
    if (newTransformMatrix != proxy->bufferData.transformMatrix)
    {
        proxy->forceRebind = true;
        proxy->bufferData.transformMatrix = newTransformMatrix;
    }
}

} // namespace hyperion
