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

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

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

    if (m_noiseTexture)
    {
        SafeDelete(std::move(m_noiseTexture));
    }
}

void FogVolume::Init()
{
    VolumeBase::Init();

    if (m_volumeTexture)
    {
        InitObject(m_volumeTexture);
    }

    if (m_noiseTexture)
    {
        InitObject(m_noiseTexture);
    }

    SetNeedsRenderProxyUpdate();

    SetReady(true);
}

void FogVolume::SetTextures(
    const Handle<Texture>& volumeTexture,
    const Handle<Texture>& noiseTexture)
{
    if (m_volumeTexture != volumeTexture)
    {
        if (m_volumeTexture)
        {
            SafeDelete(std::move(m_volumeTexture));
        }

        m_volumeTexture = volumeTexture;
    }

    if (m_noiseTexture != noiseTexture)
    {
        if (m_noiseTexture)
        {
            SafeDelete(std::move(m_noiseTexture));
        }

        m_noiseTexture = noiseTexture;
    }

    if (IsInitCalled())
    {
        InitObject(m_volumeTexture);

        SetNeedsRenderProxyUpdate();
    }
}

void FogVolume::UpdateRenderProxy(RenderProxyFogVolume* proxy)
{
    AssertDebug(proxy != nullptr);

    const BoundingBox worldAabb = GetWorldBounds();

    proxy->fogVolume = WeakHandleFromThis();
    proxy->worldAabb = worldAabb;

    if (proxy->volumeTexture != m_volumeTexture)
    {
        proxy->forceRebind = true;

        proxy->volumeTexture = m_volumeTexture;
    }

    if (proxy->noiseTexture != m_noiseTexture)
    {
        proxy->forceRebind = true;

        proxy->noiseTexture = m_noiseTexture;
    }

    // create transform matrix turning 1:1:1 cube to the world bounds
    const Vec3f boxSize = m_localBounds.GetExtent() * 0.5f;

    const Mat4f newTransformMatrix = GetWorldMatrix() * Mat4f::Scaling(boxSize);
    if (newTransformMatrix != proxy->bufferData.transformMatrix
        || worldAabb.min != proxy->bufferData.aabbMin.GetXYZ()
        || worldAabb.max != proxy->bufferData.aabbMax.GetXYZ())
    {
        proxy->forceRebind = true;

        proxy->bufferData.transformMatrix = newTransformMatrix;
        proxy->bufferData.aabbMin = Vec4f(worldAabb.min, 1.0f);
        proxy->bufferData.aabbMax = Vec4f(worldAabb.max, 1.0f);
    }
}

} // namespace hyperion
