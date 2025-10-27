/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Uuid.hpp>
#include <core/utilities/Result.hpp>

#include <rendering/lightmapper/LightmapAtlas.hpp>
#include <rendering/lightmapper/LightmapTexel.hpp>

namespace hyperion {

namespace threading { class TaskBatch; }
using threading::TaskBatch;

class Scene;
class View;
class LightmapVolume;
struct LightmapElement;
class ILightmapRenderer;

struct RenderSetup; // forward decl for renderer interface usage

enum class LightmapShadingType : int; // forward decl from Lightmapper
struct LightmapHit; // forward decl from Lightmapper

struct LightmapperConfig; // forward decl from Lightmapper

struct LightmapJobParams
{
    LightmapperConfig* config;

    Handle<Scene> scene;
    Handle<View> view;

    Span<LightmapSubElement> subElementsView;
    HashMap<Handle<Entity>, LightmapSubElement*>* subElementsByEntity;

    Array<UniquePtr<ILightmapRenderer>>* renderers = nullptr;
};

class HYP_API LightmapJobBase
{
public:
    explicit LightmapJobBase(LightmapJobParams&& params);

    LightmapJobBase(const LightmapJobBase& other) = delete;
    LightmapJobBase& operator=(const LightmapJobBase& other) = delete;

    LightmapJobBase(LightmapJobBase&& other) noexcept = delete;
    LightmapJobBase& operator=(LightmapJobBase&& other) noexcept = delete;

    virtual ~LightmapJobBase();

    HYP_FORCE_INLINE const LightmapJobParams& GetParams() const
    {
        return m_params;
    }

    HYP_FORCE_INLINE const Uuid& GetUUID() const
    {
        return m_uuid;
    }

    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_params.scene.Get();
    }

    HYP_FORCE_INLINE Span<LightmapSubElement> GetSubElements() const
    {
        return m_params.subElementsView;
    }

    HYP_FORCE_INLINE uint32 GetTexelIndex() const
    {
        return m_texelIndex;
    }

    HYP_FORCE_INLINE const Array<uint32>& GetTexelIndices() const
    {
        return m_texelIndices;
    }

    HYP_FORCE_INLINE void GetPreviousFrameRays(Array<LightmapRay>& outRays) const
    {
        Mutex::Guard guard(m_previousFrameRaysMutex);

        outRays = m_previousFrameRays;
    }

    HYP_FORCE_INLINE void SetPreviousFrameRays(const Array<LightmapRay>& rays)
    {
        Mutex::Guard guard(m_previousFrameRaysMutex);

        m_previousFrameRays = rays;
    }

    HYP_FORCE_INLINE const Result& GetResult() const
    {
        return m_result;
    }

    void Start();
    void Process();

    void AddTask(TaskBatch* taskBatch);

    /*! \brief Integrate ray hits into the lightmap.
     *  \param rays The rays that were traced.
     *  \param hits The hits to integrate.
     */
    virtual void IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shadingType);

    /*! \brief Gather next rays to be traced.
     *  \param maxRayHits The maximum number of rays to gather.
     *  \param outRays The output array to store gathered rays.
     */
    virtual void GatherRays(uint32 maxRayHits, Array<LightmapRay>& outRays);

    bool IsCompleted() const;

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_runningSemaphore.IsInSignalState();
    }

    // Number of GPU path tracing tasks running, used to not overwhelm the gpu while rendering the frame
    AtomicVar<uint32> numConcurrentRenderingTasks;

protected:
    virtual void Start_Internal() = 0;
    virtual void Process_Internal(bool* outIsReady = nullptr) = 0;

    virtual LightmapTexelsBase& GetTexels() = 0;

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

    LightmapJobParams m_params;

    Uuid m_uuid;

    Array<uint32> m_texelIndices; // flattened texel indices, flattened so that meshes are grouped together

    Array<LightmapRay> m_previousFrameRays;
    mutable Mutex m_previousFrameRaysMutex;

    Array<TaskBatch*> m_currentTasks;
    mutable Mutex m_currentTasksMutex;

    Semaphore<int32> m_runningSemaphore;
    uint32 m_texelIndex;

    double m_lastLoggedPercentage;

    Result m_result;
};

template <class T>
class LightmapJob;

template <>
class LightmapJob<LightmapVolume> : public LightmapJobBase
{
public:
    explicit LightmapJob(LightmapJobParams&& params, const Handle<LightmapVolume>& volume)
        : LightmapJobBase(std::move(params)),
          m_volume(volume),
          m_atlasBuilt(false),
          m_lightmapElement(nullptr)
    {
    }

    virtual ~LightmapJob() override;

    HYP_FORCE_INLINE const Handle<LightmapVolume>& GetVolume() const
    {
        return m_volume;
    }

    HYP_FORCE_INLINE LightmapAtlas& GetAtlas()
    {
        return m_atlas;
    }

    HYP_FORCE_INLINE const LightmapAtlas& GetAtlas() const
    {
        return m_atlas;
    }

    HYP_FORCE_INLINE LightmapElement* GetLightmapElement() const
    {
        return m_lightmapElement;
    }

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    virtual LightmapTexelsBase& GetTexels() override
    {
        return m_atlas;
    }

    Handle<LightmapVolume> m_volume;

    LightmapAtlas m_atlas;
    Task<Result> m_atlasBuildTask;
    bool m_atlasBuilt;

    LightmapElement* m_lightmapElement;
};

} // namespace hyperion
