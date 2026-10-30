/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Texture;
class GlimmerSceneInstances;
class GlimmerTriangleCache;

static constexpr uint32 GlimmerSHOccupancyGridXZ = GlimmerSHGridXZ * 2;
static constexpr uint32 GlimmerSHOccupancyGridY = GlimmerSHGridY * 2;
static constexpr uint32 GlimmerSHOccupancyMaskWords = 2;

static constexpr uint32 GlimmerSHOccupancyTracedCascades = 2;
static constexpr uint32 GlimmerSHOccupancyLightmapFaces = 6;

struct GlimmerSHOccupancyCascadeShaderData
{
    /// xyz = the window's first voxel position
    /// w = 1 once built
    Vec4i origin;

    /// x = voxel spacing
    /// y = 1 / spacing
    Vec4f params;
};

struct GlimmerSHOccupancyShaderData
{
    GlimmerSHOccupancyCascadeShaderData cascades[GlimmerSHCascades];
};

class GlimmerSHOccupancy final
{
public:
    GlimmerSHOccupancy();

    GlimmerSHOccupancy(const GlimmerSHOccupancy& other) = delete;
    GlimmerSHOccupancy& operator=(const GlimmerSHOccupancy& other) = delete;
    
    ~GlimmerSHOccupancy();

    void Update(
        Frame* frame,
        const Vec3f& viewerPosition,
        const Vec3f& lookAhead,
        const GlimmerSceneInstances& sceneInstances,
        const GlimmerTriangleCache& triangleCache,
        const GlimmerLightmapPages& lightmapPages,
        RWStructuredBuffer& scratchBuffer);

    HYP_FORCE_INLINE const GlimmerSHOccupancyShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    // false while a cascade is still waiting to be rebuilt for the current scene instances or window
    HYP_FORCE_INLINE bool IsSettled() const
    {
        return m_isSettled;
    }

    const GpuImageViewRef& GetImageView() const;

    HYP_FORCE_INLINE const RWStructuredBuffer& GetMaskBuffer() const
    {
        return m_maskBuffer;
    }

private:
    void CreateResources();

    void SplatBox(
        Frame* frame,
        uint32 cascadeIndex,
        const Vec3i& origin,
        const GlimmerBox& box,
        const GlimmerSceneInstances& sceneInstances,
        const GlimmerTriangleCache& triangleCache);

    void ScrollWindow(
        Frame* frame,
        uint32 cascadeIndex,
        const Vec3i& origin,
        const GlimmerSceneInstances& sceneInstances,
        const GlimmerTriangleCache& triangleCache);
    
    void SetBuilt(uint32 cascadeIndex, const Vec3i& origin);

    Handle<Texture> m_texture;
    RWStructuredBuffer m_maskBuffer;
    const RWStructuredBuffer* m_albedoSumsBuffer; // the technique's scratch buffer, for the frame it is splatted in

    GlimmerLightmapPages m_lightmapPages;

    FixedArray<Vec3i, GlimmerSHCascades> m_builtOrigins;
    FixedArray<uint32, GlimmerSHCascades> m_builtGenerations;
    FixedArray<int32, GlimmerSHCascades> m_rebuildSlices; // next slice of a rebuild spread over frames, -1 when there's none
    FixedArray<uint32, GlimmerSHCascades> m_rebuildGenerations;
    FixedArray<Array<GlimmerBox, GlimmerAllocator>, GlimmerSHCascades> m_dirtyBoxes;

    GlimmerSHOccupancyShaderData m_shaderData;
    bool m_isSettled;
};

} // namespace Hyperion
