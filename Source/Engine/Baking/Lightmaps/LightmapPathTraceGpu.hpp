/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/Baker.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>

#include <Core/Memory/RefCountedPtr.hpp>

namespace Hyperion {

class RenderProxyList;
struct GpuLightmapperReadyNotification;

namespace Baking {

class ENGINE_API LightmapRenderer_GpuPathTracing : public ILightmapRenderer
{
public:
    LightmapRenderer_GpuPathTracing(
        BakerBase* lightmapper,
        const Handle<Scene>& scene,
        LightmapShadingType shadingType,
        uint32 maxTexelsPerFrame);
    LightmapRenderer_GpuPathTracing(const LightmapRenderer_GpuPathTracing& other) = delete;
    LightmapRenderer_GpuPathTracing& operator=(const LightmapRenderer_GpuPathTracing& other) = delete;
    LightmapRenderer_GpuPathTracing(LightmapRenderer_GpuPathTracing&& other) noexcept = delete;
    LightmapRenderer_GpuPathTracing& operator=(LightmapRenderer_GpuPathTracing&& other) noexcept = delete;
    virtual ~LightmapRenderer_GpuPathTracing() override;

    virtual uint32 MaxTexelsPerFrame() const override
    {
        return uint32(-1);
    }

    virtual LightmapShadingType GetShadingType() const override
    {
        return m_shadingType;
    }

    virtual bool CanRender() const override;

    virtual void Create() override;
    virtual void CleanJobData(BakeJobBase* job) override;
    virtual void ReadHitsBuffer(Frame* frame, BakeJobBase* job, size_t count, Proc<void(Span<LightmapHit> hits)>&& callback) override;
    virtual void Render(Frame* frame, const RenderSetup& renderSetup, BakeJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset) override;

private:
    struct JobData
    {
        GpuBufferRef cbuffer;
        GpuBufferRef raysBuffer;
        RWStructuredBuffer hitsBufferGpu;
        bool isCreated = false;
    };

    void UpdatePipelineState(Frame* frame, BakeJobBase* job);
    void CreateBuffers(BakeJobBase* job);
    void CreateAccelerationStructures();

    Handle<Scene> m_scene;
    LightmapShadingType m_shadingType;
    uint32 m_maxTexelsPerFrame;

    TMap<BakeJobBase*, JobData> m_jobData;

    RC<GpuLightmapperReadyNotification> m_readyNotification;

    GpuTlasRef m_tlas;
};

} // namespace Baking

} // namespace Hyperion
