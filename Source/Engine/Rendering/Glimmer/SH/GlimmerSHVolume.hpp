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

static constexpr float GlimmerSHSpacing = 4.0f;
static constexpr uint32 GlimmerSHDataSlabs = 2 + (8 * 8) / 2;

struct GlimmerSHCascadeShaderData
{
    Vec4i origin; // xyz = absolute voxel of the window's first voxel, w = 1 once the cascade has a window
    Vec4f params; // x = voxel spacing, y = 1 / spacing, z = how much visibility counts (Rendering.Glimmer.Visibility)
};

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

class GlimmerSHVolume final
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
