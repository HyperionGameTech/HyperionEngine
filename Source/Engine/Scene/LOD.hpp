/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Mat4f.hpp>
#include <Core/Math/BoundingSphere.hpp>

#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <Core/Reflection/ObjId.hpp>

#include <Framework/CVarManager.hpp>
#include <Framework/EngineMemory.hpp>

namespace Hyperion {

class Camera;
class Entity;
struct MeshDesc;

extern ENGINE_API CVar<int32> g_cvMeshLodForceLod;

struct LODViewData
{
    Vec3f position;

    float projectionScale = 1.0f;
    float nearClip = 0.01f;

    bool isOrthographic = false;

    LODViewData() = default;
    LODViewData(const Mat4f& projectionMatrix, const Vec3f& position, float nearClip);
    explicit LODViewData(const Camera& camera);

    inline float ComputeScreenSize(const BoundingSphere& sphere) const
    {
        if (sphere.radius <= 0.0f)
        {
            return 0.0f;
        }

        if (isOrthographic)
        {
            return sphere.radius * projectionScale;
        }

        const float distance = MathUtil::Max(position.Distance(sphere.center), nearClip);

        return sphere.radius * projectionScale / distance;
    }
};

/*! \brief The per-entity inputs to mesh LOD selection, which are the same no matter which view is selecting. */
struct MeshLodSelectionParams
{
    uint8 numLods = 1;

    ///0 == automatic lod selection, otherwise uses this minus one
    uint8 forcedLod = 0;

    int8 lodBias = 0;
};

/*! \brief Chooses the LOD a view should render a mesh at, for a mesh covering \ref{screenSize} of that view.
 *  \param previousLod The LOD the same view chose for this mesh last frame, used for hysteresis so a mesh sitting
 *   on a threshold doesn't flip back and forth every frame.
 *  \param viewLodBias Bias for this particular view, on top of the per-mesh and global ones. Shadow views pass
 *   Rendering.MeshLod.ShadowBias through here, since they can usually afford coarser geometry than the view
 *   that sees it directly.
 *
 *  Tuned by the Rendering.MeshLod.* CVars: ForceLod, ScreenSizeScale, Hysteresis, GlobalBias, MinLod, MaxLod
 *  and ShadowBias. */
ENGINE_API uint8 SelectMeshLod(const MeshDesc& meshDesc, const MeshLodSelectionParams& params, float screenSize, uint8 previousLod, int32 viewLodBias = 0);

/*! \brief Overrides the LOD every view renders one entity at, regardless of how large it is on screen.
 *  Used by the editor's mesh edit mode so the LOD being edited is the one drawn. Only one entity can be
 *  overridden at a time; setting a new one replaces the previous.
 *  \note Safe to call from any thread. */
ENGINE_API void SetMeshLodOverride(ObjId<Entity> entityId, uint8 lodIndex);

ENGINE_API void ClearMeshLodOverride();

/*! \brief Returns the LOD \ref{entityId} is pinned to, or ~0 if it isn't the overridden entity. */
ENGINE_API uint8 GetMeshLodOverride(ObjId<Entity> entityId);

} // namespace Hyperion
