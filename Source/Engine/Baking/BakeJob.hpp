/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Task.hpp>
#include <Core/Threading/Semaphore.hpp>
#include <Core/Threading/ThreadSignal.hpp>

#include <Core/Utilities/Span.hpp>
#include <Core/Utilities/Uuid.hpp>
#include <Core/Utilities/Result.hpp>

#include <Baking/BakeData.hpp>
#include <Baking/LightmapTexel.hpp>

namespace Hyperion {

namespace threading {
class TaskBatch;
}
using threading::TaskBatch;

class Scene;
class ReflectionProbe;
class FogVolume;
class Light;
class View;
class LightmapVolume;
struct LightmapElement;

struct RenderSetup; // forward decl for renderer interface usage

namespace Baking {

class BakerBase;

enum class LightmapShadingType : uint32; // forward decl from Lightmapper
struct LightmapHit;                     // forward decl from Lightmapper

struct BakerConfig; // forward decl from Lightmapper
class ILightmapRenderer;

struct BakeJobParams
{
    BakerConfig* config;

    Handle<Scene> scene;
    Handle<View> view;

    Span<BakeEntity> bakeEntitiesView;
    TMap<Handle<Entity>, BakeEntity*>* bakeEntitiesByEntity;

    Array<UniquePtr<ILightmapRenderer>>* renderers = nullptr;
};

class ENGINE_API BakeJobBase
{
    friend class BakerBase;

public:
    explicit BakeJobBase(BakeJobParams&& params);

    BakeJobBase(const BakeJobBase& other) = delete;
    BakeJobBase& operator=(const BakeJobBase& other) = delete;

    BakeJobBase(BakeJobBase&& other) noexcept = delete;
    BakeJobBase& operator=(BakeJobBase&& other) noexcept = delete;

    virtual ~BakeJobBase();

    HYP_FORCE_INLINE const BakeJobParams& GetParams() const
    {
        return m_params;
    }

    HYP_FORCE_INLINE const UUID& GetUUID() const
    {
        return m_uuid;
    }

    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_params.scene.Get();
    }

    HYP_FORCE_INLINE Span<BakeEntity> GetBakeEntities() const
    {
        return m_params.bakeEntitiesView;
    }

    HYP_FORCE_INLINE uint32 GetTexelIndex() const
    {
        return m_texelIndex;
    }

    HYP_FORCE_INLINE const Array<uint32>& GetTexelIndices() const
    {
        return m_texelIndices;
    }

    void SetTexelIndices(Array<uint32>&& texelIndices)
    {
        m_texelIndices = std::move(texelIndices);
    }

    HYP_FORCE_INLINE void GetPreviousFrameRays(Array<LightmapRay>& outRays) const
    {
        TSharedLock lock(m_previousFrameRaysMutex);

        outRays = m_previousFrameRays;
    }

    HYP_FORCE_INLINE void SetPreviousFrameRays(const Array<LightmapRay>& rays)
    {
        TUniqueLock lock(m_previousFrameRaysMutex);

        m_previousFrameRays = rays;
    }

    const Result& GetResult() const
    {
        return m_result;
    }

    void Start();
    uint32 Process(uint32 maxTexels = ~0u);

    void AddTask(TaskBatch* taskBatch);

    /*! \brief Integrate ray hits into the lightmap.
     *  \param rays The rays that were traced.
     *  \param hits The hits to integrate.
     */
    virtual void IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shadingType);

    /*! \brief Gather next texels to process.
     *  \param maxTexels Maximum number of texels to gather.
     *  \param outTexels Output array to store gathered texels (pointers).
     */
    virtual void GatherTexels(uint32 maxTexels, Array<LightmapTexel*>& outTexels);
    virtual uint32 ProcessTexels(Span<LightmapTexel*> texels, uint32 texelOffset = 0);

    bool IsCompleted() const;

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_runningSemaphore.IsInSignalState();
    }

    // Lightmap tracing only
    ThreadSignal tracingCompleteSignal;
    ByteBuffer readbackData;

protected:
    virtual void Start_Internal() = 0;
    virtual void Process_Internal(bool* outIsReady = nullptr) = 0;

    virtual BakeDataBase& GetBakeData() = 0;

    HYP_FORCE_INLINE const BakeDataBase& GetBakeData() const
    {
        return const_cast<BakeJobBase*>(this)->GetBakeData();
    }

    bool HasRemainingTexels() const;

    /*! \brief Get the next texel index to process, advancing the teexl counter
     *  \return The texel index
     */
    HYP_FORCE_INLINE uint32 NextTexel()
    {
        const uint32 currentTexelIndex = m_texelIndices[m_texelIndex % m_texelIndices.Size()];
        m_texelIndex++;

        return currentTexelIndex;
    }

    void Stop();
    void Stop(const Error& error);

    BakerBase* m_baker;

    BakeJobParams m_params;

    UUID m_uuid;

    Array<uint32> m_texelIndices; // flattened texel indices, flattened so that meshes are grouped together

    Array<LightmapRay> m_previousFrameRays;
    SharedMutex m_previousFrameRaysMutex;

    Array<TaskBatch*> m_currentTasks;
    SharedMutex m_currentTasksMutex;

    Semaphore<int32> m_runningSemaphore;
    uint32 m_texelIndex;

    double m_lastLoggedPercentage;

    bool m_wasStarted;

    Result m_result;
};

template <class T>
class BakeJob;

} // namespace Baking

} // namespace Hyperion
