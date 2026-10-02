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
class EnvProbe;
class CloudPass;
class GlimmerSurfaceCache;
class GlimmerSpanCache;
class GlimmerTLAS;
class GlimmerSWRTProbeVolume;
struct GlimmerSHOccupancyShaderData;
struct GlimmerRelightShaderData;

static constexpr uint32 GlimmerSHCascades = 5;
static constexpr uint32 GlimmerSHGridXZ = 32;
static constexpr uint32 GlimmerSHGridY = 16;

static constexpr float GlimmerSHSpacing = 4.0f;

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
    const GlimmerTLAS* tlas = nullptr;

    const GlimmerSHOccupancyShaderData* occupancy = nullptr;
    GpuImageViewRef occupancyImageView;
    bool isOccupancySettled = true; // voxels traced before then would keep stale solids until their refresh

    const GlimmerRelightShaderData* relight = nullptr;
    GpuImageViewRef relightImageView;

    // the hits are lit as the probe trace lights its own: the near field (last frame's) where it covers them, the sky and the sun
    // through the clouds; any may be nullptr
    const GlimmerSWRTProbeVolume* probeVolume = nullptr;
    EnvProbe* skyProbe = nullptr;
    const CloudPass* cloudPass = nullptr;
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
    const GpuImageViewRef& GetRadianceImageView() const;

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
    void AddPending(uint32 cascadeIndex, const Box& box);
    void AddPendingWorld(const Vec3f& worldMin, const Vec3f& worldMax, int32 marginVoxels);
    void DispatchBox(Frame* frame, uint32 cascadeIndex, const Box& box, const GlimmerSHVolumeUpdateInputs& inputs, bool& inOutHasBarriers);

    Handle<Texture> m_dataTexture;     // sky visibility L1
    Handle<Texture> m_stateTexture;
    Handle<Texture> m_radianceTexture; // L1 of incoming radiance; R, G and B stacked along y

    FixedArray<Cascade, GlimmerSHCascades> m_cascades;

    uint32 m_refreshStep;
    FixedArray<int32, GlimmerSHCascades> m_refreshSlices;
    uint32 m_updateIndex;
    uint32 m_seenTLASGeneration;
    uint32 m_occupancyWaitFrames;

    GlimmerSHVolumeShaderData m_shaderData;
};

} // namespace Hyperion
