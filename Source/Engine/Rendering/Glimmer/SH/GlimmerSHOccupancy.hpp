/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Texture;
class GlimmerTLAS;
class GlimmerBLASCache;

// one occupancy cascade per SH cascade, covering the same region at half its voxel spacing
static constexpr uint32 GlimmerSHOccupancyGridXZ = GlimmerSHGridXZ * 2;
static constexpr uint32 GlimmerSHOccupancyGridY = GlimmerSHGridY * 2;

// Must match GlimmerSHOccupancyCascade in Shaders/Glimmer/SH/GlimmerSHOccupancy.hlsli
struct GlimmerSHOccupancyCascadeShaderData
{
    Vec4i origin; // xyz = absolute voxel of the window's first voxel, w = 1 once built
    Vec4f params; // x = voxel spacing, y = 1 / spacing
};

// Must match GlimmerSHOccupancyParams in Shaders/Glimmer/SH/GlimmerSHOccupancy.hlsli
struct GlimmerSHOccupancyShaderData
{
    GlimmerSHOccupancyCascadeShaderData cascades[GlimmerSHCascades];
};

/*! \brief Clipmap of which voxels around the viewer the scene's solids fill, and their albedo, so SH rays see overhangs (arches,
 *  galleries, roofs) that the heightfield's one span per column can't. Built on the GPU by splatting the solid span instances' BLAS
 *  triangles; a cascade is rebuilt whole when its window moves or the scene's instances change, one cascade per frame.
 *  Render thread only. */
class GlimmerSHOccupancy
{
public:
    GlimmerSHOccupancy();
    GlimmerSHOccupancy(const GlimmerSHOccupancy& other) = delete;
    GlimmerSHOccupancy& operator=(const GlimmerSHOccupancy& other) = delete;
    ~GlimmerSHOccupancy();

    void Update(Frame* frame, const Vec3f& viewerPosition, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache);

    HYP_FORCE_INLINE const GlimmerSHOccupancyShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    /*! \brief Texture3D<float4> of GridXZ x (Cascades * GridY) x GridXZ: rgb = albedo, a = 1 where filled. */
    const GpuImageViewRef& GetImageView() const;

private:
    void CreateResources();
    void RebuildCascade(Frame* frame, uint32 cascadeIndex, const Vec3i& origin, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache);

    Handle<Texture> m_texture;

    FixedArray<Vec3i, GlimmerSHCascades> m_builtOrigins;
    FixedArray<uint32, GlimmerSHCascades> m_builtGenerations;

    GlimmerSHOccupancyShaderData m_shaderData;
};

} // namespace Hyperion
