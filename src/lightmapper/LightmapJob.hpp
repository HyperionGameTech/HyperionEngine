/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Uuid.hpp>
#include <core/utilities/Result.hpp>

#include <lightmapper/LightmapData.hpp>
#include <lightmapper/LightmapTexel.hpp>

namespace hyperion {

namespace threading {
class TaskBatch;
}
using threading::TaskBatch;

class Scene;
class ReflectionProbe;
class FogVolume;
class View;
class LightmapVolume;
struct LightmapElement;
class ILightmapRenderer;
class LightmapperBase;

struct RenderSetup; // forward decl for renderer interface usage

enum class LightmapShadingType : int; // forward decl from Lightmapper
struct LightmapHit;                   // forward decl from Lightmapper

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
    friend class LightmapperBase;

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

    void SetTexelIndices(Array<uint32>&& texelIndices)
    {
        m_texelIndices = std::move(texelIndices);
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

    // Number of GPU path tracing tasks running, used to not overwhelm the gpu while rendering the frame
    AtomicVar<uint32> NumConcurrentRenderingTasks;

protected:
    virtual void Start_Internal() = 0;
    virtual void Process_Internal(bool* outIsReady = nullptr) = 0;

    virtual LightmapDataBase& GetLightmapData() = 0;

    HYP_FORCE_INLINE const LightmapDataBase& GetLightmapData() const
    {
        return const_cast<LightmapJobBase*>(this)->GetLightmapData();
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

    LightmapperBase* m_lightmapper;

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

    bool m_wasStarted;

    Result m_result;
};

template <class T>
class LightmapJob;

template <>
class LightmapJob<LightmapVolume> : public LightmapJobBase
{
public:
    LightmapJob(LightmapJobParams&& params, const Handle<LightmapVolume>& volume, LightmapData<LightmapVolume>* pLightmapData);
    virtual ~LightmapJob() override;

    HYP_FORCE_INLINE const Handle<LightmapVolume>& GetVolume() const
    {
        return m_volume;
    }

    virtual LightmapData<LightmapVolume>& GetLightmapData() override
    {
        return *m_pLightmapData;
    }

    HYP_FORCE_INLINE LightmapElement* GetLightmapElement() const
    {
        return m_lightmapElement;
    }

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<LightmapVolume> m_volume;

    LightmapData<LightmapVolume>* m_pLightmapData;

    LightmapElement* m_lightmapElement;
};

template <>
class LightmapJob<ReflectionProbe> : public LightmapJobBase
{
public:
    explicit LightmapJob(LightmapJobParams&& params, const Handle<ReflectionProbe>& envProbe, LightmapData<ReflectionProbe>* pLightmapData)
        : LightmapJobBase(std::move(params)),
          m_envProbe(envProbe),
          m_pLightmapData(pLightmapData)
    {
    }

    virtual ~LightmapJob() override;

    HYP_FORCE_INLINE const Handle<ReflectionProbe>& GetEnvProbe() const
    {
        return m_envProbe;
    }

    virtual LightmapData<ReflectionProbe>& GetLightmapData() override
    {
        return *m_pLightmapData;
    }

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<ReflectionProbe> m_envProbe;
    LightmapData<ReflectionProbe>* m_pLightmapData;
};

template <>
class LightmapJob<FogVolume> : public LightmapJobBase
{
public:
    explicit LightmapJob(LightmapJobParams&& params, const Handle<FogVolume>& fogVolume, LightmapData<FogVolume>* pLightmapData)
        : LightmapJobBase(std::move(params)),
          m_fogVolume(fogVolume),
          m_pLightmapData(pLightmapData)
    {
    }

    virtual ~LightmapJob() override;

    HYP_FORCE_INLINE const Handle<FogVolume>& GetFogVolume() const
    {
        return m_fogVolume;
    }

    virtual LightmapData<FogVolume>& GetLightmapData() override
    {
        return *m_pLightmapData;
    }

    virtual uint32 ProcessTexels(Span<LightmapTexel*> texels, uint32 texelOffset = 0) override;

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<FogVolume> m_fogVolume;
    LightmapData<FogVolume>* m_pLightmapData;
};

} // namespace hyperion
