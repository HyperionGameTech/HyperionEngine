/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Uuid.hpp>
#include <core/utilities/Result.hpp>

#include <baking/BakeData.hpp>
#include <baking/LightmapTexel.hpp>

namespace Hyperion {

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
class BakerBase;

struct RenderSetup; // forward decl for renderer interface usage

namespace Baking {

enum class LightmapShadingType : int; // forward decl from Lightmapper
struct LightmapHit;                   // forward decl from Lightmapper

struct LightmapperConfig; // forward decl from Lightmapper

struct BakeJobParams
{
    LightmapperConfig* config;

    Handle<Scene> scene;
    Handle<View> view;

    Span<LightmapSubElement> subElementsView;
    HashMap<Handle<Entity>, LightmapSubElement*>* subElementsByEntity;

    Array<UniquePtr<ILightmapRenderer>>* renderers = nullptr;
};

class HYP_API BakeJobBase
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
        return const_cast<BakeJobBase*>(this)->GetLightmapData();
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

    BakerBase* m_lightmapper;

    BakeJobParams m_params;

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
class BakeJob;

template <>
class BakeJob<LightmapVolume> : public BakeJobBase
{
public:
    BakeJob(BakeJobParams&& params, const Handle<LightmapVolume>& volume, BakeData<LightmapVolume>* lightmapData);
    virtual ~BakeJob() override;

    HYP_FORCE_INLINE const Handle<LightmapVolume>& GetVolume() const
    {
        return m_volume;
    }

    virtual BakeData<LightmapVolume>& GetLightmapData() override
    {
        return *m_lightmapData;
    }

    HYP_FORCE_INLINE LightmapElement* GetLightmapElement() const
    {
        return m_lightmapElement;
    }

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<LightmapVolume> m_volume;

    BakeData<LightmapVolume>* m_lightmapData;

    LightmapElement* m_lightmapElement;
};

template <>
class BakeJob<ReflectionProbe> : public BakeJobBase
{
public:
    explicit BakeJob(BakeJobParams&& params, const Handle<ReflectionProbe>& envProbe, BakeData<ReflectionProbe>* lightmapData)
        : BakeJobBase(std::move(params)),
          m_envProbe(envProbe),
          m_lightmapData(lightmapData)
    {
    }

    virtual ~BakeJob() override;

    HYP_FORCE_INLINE const Handle<ReflectionProbe>& GetEnvProbe() const
    {
        return m_envProbe;
    }

    virtual BakeData<ReflectionProbe>& GetLightmapData() override
    {
        return *m_lightmapData;
    }

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<ReflectionProbe> m_envProbe;
    BakeData<ReflectionProbe>* m_lightmapData;
};

template <>
class BakeJob<FogVolume> : public BakeJobBase
{
public:
    explicit BakeJob(BakeJobParams&& params, const Handle<FogVolume>& fogVolume, BakeData<FogVolume>* lightmapData)
        : BakeJobBase(std::move(params)),
          m_fogVolume(fogVolume),
          m_lightmapData(lightmapData)
    {
    }

    virtual ~BakeJob() override;

    HYP_FORCE_INLINE const Handle<FogVolume>& GetFogVolume() const
    {
        return m_fogVolume;
    }

    virtual BakeData<FogVolume>& GetLightmapData() override
    {
        return *m_lightmapData;
    }

    virtual uint32 ProcessTexels(Span<LightmapTexel*> texels, uint32 texelOffset = 0) override;

protected:
    virtual void Start_Internal() override;
    virtual void Process_Internal(bool* outIsReadyToProcess) override;

    Handle<FogVolume> m_fogVolume;
    BakeData<FogVolume>* m_lightmapData;
};

} // namespace Baking

} // namespace Hyperion
