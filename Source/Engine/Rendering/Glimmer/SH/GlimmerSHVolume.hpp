/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>

#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/Utilities/Optional.hpp>

namespace Hyperion {

class Texture;
class EnvProbe;
class CloudPass;
class GlimmerSurfaceCache;
class GlimmerSpanCache;
class GlimmerSceneInstances;
class GlimmerProbeVolume;
class GlimmerLocalLights;
struct GlimmerSHOccupancyShaderData;
struct GlimmerRelightShaderData;

static constexpr uint32 GlimmerSHCascades = 5;
static constexpr uint32 GlimmerSHGridXZ = 32;
static constexpr uint32 GlimmerSHGridY = 16;

static constexpr float GlimmerSHSpacing = 4.0f;

struct GlimmerSHCascadeShaderData
{
    /// xyz = window's first voxel pos
    /// w = cascade has a window?
    Vec4i origin;

    /// x = voxel spacing
    /// y = 1 / spacing
    /// z = Rendering.Glimmer.Visibility enabled?
    /// w = finest ground level its traces read
    Vec4f params;

    /// xyz = the window's center before snapping, in voxels
    Vec4f center;
};

struct GlimmerSHVolumeShaderData
{
    GlimmerSHCascadeShaderData cascades[GlimmerSHCascades];

    /// x = number of cascades
    /// w = 1 when anything has been traced
    Vec4u info;
};

struct GlimmerSHVolumeUpdateInputs
{
    Vec3f viewerPosition;
    Vec3f lookAhead;          // GlimmerChannelState::lookAhead
    float budgetScale = 1.0f; // GlimmerChannelState::budgetScale
    const GlimmerSurfaceCache* surfaceCache = nullptr;
    const GlimmerSpanCache* spanCache = nullptr;
    const GlimmerSceneInstances* sceneInstances = nullptr;

    const GlimmerSHOccupancyShaderData* occupancy = nullptr;
    GpuImageViewRef occupancyImageView;
    const RWStructuredBuffer* occupancyMaskBuffer = nullptr;
    bool isOccupancySettled = true;

    const GlimmerRelightShaderData* relight = nullptr;
    GpuImageViewRef relightImageView;

    const GlimmerProbeVolume* probeVolume = nullptr;

    EnvProbe* skyProbe = nullptr;
    const CloudPass* cloudPass = nullptr;

    GlimmerLightShaderData light;

    const GlimmerLocalLights* localLights = nullptr;
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

    HYP_FORCE_INLINE const RWStructuredBuffer& GetLightBuffer() const
    {
        return m_lightBuffer;
    }

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

        HYP_FORCE_INLINE int64 Volume() const
        {
            return IsEmpty() ? 0 : int64(max.x - min.x) * int64(max.y - min.y) * int64(max.z - min.z);
        }

        static Box Intersect(const Box& a, const Box& b);
        static Box Union(const Box& a, const Box& b);
    };

    struct Cascade
    {
        Optional<Vec3i> origin;
        Array<Box, GlimmerAllocator> pending;

        ///        box, passesRemaining
        Array<Pair<Box, int32>, GlimmerAllocator> accumulating; // traced once already, waiting for the rest of their passes
    };

    static Box GetWindow(const Vec3i& origin);

    void CreateResources();
    void MoveWindow(uint32 cascadeIndex, const Vec3i& origin);
    void AddPending(uint32 cascadeIndex, const Box& box);
    void AddPendingWorld(const Vec3f& worldMin, const Vec3f& worldMax, int32 marginVoxels, uint32 numCascades = GlimmerSHCascades);
    void QueuePasses(uint32 cascadeIndex, const Box& box);
    void DispatchBox(Frame* frame, uint32 cascadeIndex, const Box& box, float radianceHysteresis, const GlimmerSHVolumeUpdateInputs& inputs, bool& inOutHasBarriers);

    /// returns false when nothing was dispatched: the cascade can't be refreshed yet, or what was left of its pass scrolled out
    bool DispatchRefreshSlice(
        Frame* frame,
        uint32 cascadeIndex,
        int32& inOutSlice,
        float radianceHysteresis,
        bool& outFinishedCascade,
        const GlimmerSHVolumeUpdateInputs& inputs,
        bool& inOutHasBarriers);

    void Compose(Frame* frame, const GlimmerLightShaderData& light);

    Handle<Texture> m_dataTexture;      // sky visibility L1
    Handle<Texture> m_stateTexture;
    Handle<Texture> m_radianceTexture;  // L1 of incoming radiance; R, G and B side by side along x
    RWStructuredBuffer m_lightBuffer;

    FixedArray<Cascade, GlimmerSHCascades> m_cascades;

    uint32 m_refreshStep;
    FixedArray<int32, GlimmerSHCascades> m_refreshSlices; // absolute, 2 per voxel along z
    uint32 m_updateIndex;
    uint32 m_seenInstancesGeneration;
    uint32 m_seenLightsGeneration = 0;
    uint32 m_occupancyWaitFrames;

    int32 m_refreshBudget;                              /// voxels the refresh may still trace, carried between frames so a rate below a slice still gets its turn

    GlimmerLightShaderData m_composedLight;
    bool m_hasComposedLight;

    GlimmerSHVolumeShaderData m_shaderData;
};

} // namespace Hyperion
