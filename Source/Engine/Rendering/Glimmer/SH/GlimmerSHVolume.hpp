/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Texture;
class GlimmerSurfaceCache;
class GlimmerSpanCache;
struct GlimmerSHOccupancyShaderData;

static constexpr uint32 GlimmerSHCascades = 5;
static constexpr uint32 GlimmerSHGridXZ = 32;
static constexpr uint32 GlimmerSHGridY = 16;
static constexpr uint32 GlimmerSHRays = 64;
// cascade 0, doubling per cascade: 4 m voxels (a +-64 m window) light everything near the viewer that the probe blocks don't reach,
// open terrain included; the coarsest reaches +-1 km
static constexpr float GlimmerSHSpacing = 4.0f;
// visibility, bounce and depth x/y/z share one texture, a GlimmerSHGridXZ deep slab each. Must match GLIMMER_SH_SLABS in Shaders/Glimmer/SH/GlimmerSHCommon.hlsli
static constexpr uint32 GlimmerSHDataSlabs = 5;

// Must match GlimmerSHCascade in Shaders/Glimmer/SH/GlimmerSHCommon.hlsli
struct GlimmerSHCascadeShaderData
{
    Vec4i origin; // xyz = absolute voxel of the window's first voxel, w = 1 once the cascade has a window
    Vec4f params; // x = voxel spacing, y = 1 / spacing
};

// Must match GlimmerSHVolume in Shaders/Glimmer/SH/GlimmerSHCommon.hlsli
struct GlimmerSHVolumeShaderData
{
    GlimmerSHCascadeShaderData cascades[GlimmerSHCascades];
    Vec4u info; // x = number of cascades, w = 1 when anything has been traced
};

struct GlimmerSHVolumeUpdateInputs
{
    Vec3f viewerPosition;
    const GlimmerSurfaceCache* surfaceCache = nullptr;
    const GlimmerSpanCache* spanCache = nullptr;

    const GlimmerSHOccupancyShaderData* occupancy = nullptr;
    GpuImageViewRef occupancyImageView;
};

/*! \brief Clipmap of voxels around the viewer, each holding what it sees of the sky and of the surfaces blocking it:
 *  L1 sky visibility (canopy transmittance included), the blockers' mean albedo and how much of them the sun lights,
 *  and per axis direction the distance moments to the nearest solid, which keep light from leaking through walls when interpolating.
 *  Voxels are traced against the occupancy clipmap and the heightfield (ground + spans) only when they scroll in, plus a slow round robin refresh,
 *  within a per frame budget; lighting relights them with the current sky and sun per pixel. Render thread only. */
class GlimmerSHVolume
{
public:
    GlimmerSHVolume();
    GlimmerSHVolume(const GlimmerSHVolume& other) = delete;
    GlimmerSHVolume& operator=(const GlimmerSHVolume& other) = delete;
    ~GlimmerSHVolume();

    void Update(Frame* frame, const GlimmerSHVolumeUpdateInputs& inputs);

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_shaderData.info.w != 0;
    }

    HYP_FORCE_INLINE const GlimmerSHVolumeShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    /*! \brief Visibility, bounce and depth x/y/z, stacked along z (see GlimmerSHCommon.hlsli) */
    const GpuImageViewRef& GetDataImageView() const;
    const GpuImageViewRef& GetStateImageView() const;

private:
    struct Box
    {
        Vec3i min;
        Vec3i max; // exclusive

        HYP_FORCE_INLINE bool IsEmpty() const
        {
            return max.x <= min.x || max.y <= min.y || max.z <= min.z;
        }

        HYP_FORCE_INLINE Vec3i Extent() const
        {
            return max - min;
        }

        static Box Intersect(const Box& a, const Box& b);
    };

    struct Cascade
    {
        bool hasOrigin = false;
        Vec3i origin;
        Array<Box> pending;
    };

    static Box GetWindow(const Vec3i& origin);

    void CreateResources();
    void MoveWindow(uint32 cascadeIndex, const Vec3i& origin);
    void DispatchBox(Frame* frame, uint32 cascadeIndex, const Box& box, const GlimmerSHVolumeUpdateInputs& inputs, bool& inOutHasBarriers);

    Handle<Texture> m_dataTexture;
    Handle<Texture> m_stateTexture;

    FixedArray<Cascade, GlimmerSHCascades> m_cascades;

    uint32 m_refreshCascade;
    int32 m_refreshSlice;
    uint32 m_updateIndex;

    GlimmerSHVolumeShaderData m_shaderData;
};

} // namespace Hyperion
