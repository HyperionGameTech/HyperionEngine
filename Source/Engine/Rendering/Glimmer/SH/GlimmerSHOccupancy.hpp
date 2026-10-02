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

static constexpr uint32 GlimmerSHOccupancyGridXZ = GlimmerSHGridXZ * 2;
static constexpr uint32 GlimmerSHOccupancyGridY = GlimmerSHGridY * 2;

struct GlimmerSHOccupancyCascadeShaderData
{
    Vec4i origin; // xyz = absolute voxel of the window's first voxel, w = 1 once built
    Vec4f params; // x = voxel spacing, y = 1 / spacing
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

    void Update(Frame* frame, const Vec3f& viewerPosition, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache);

    HYP_FORCE_INLINE const GlimmerSHOccupancyShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    // false while a cascade is still waiting to be rebuilt for the current TLAS or window
    HYP_FORCE_INLINE bool IsSettled() const
    {
        return m_isSettled;
    }

    const GpuImageViewRef& GetImageView() const;

private:
    void CreateResources();
    struct Box
    {
        Vec3i min;
        Vec3i max; // exclusive
    };

    void SplatBox(Frame* frame, uint32 cascadeIndex, const Vec3i& origin, const Box& box, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache);

    Handle<Texture> m_texture;

    FixedArray<Vec3i, GlimmerSHCascades> m_builtOrigins;
    FixedArray<uint32, GlimmerSHCascades> m_builtGenerations;
    FixedArray<Array<Box>, GlimmerSHCascades> m_dirtyBoxes;

    GlimmerSHOccupancyShaderData m_shaderData;
    bool m_isSettled;
};

} // namespace Hyperion
