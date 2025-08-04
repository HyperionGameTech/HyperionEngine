/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/Threads.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>

#include <util/img/Bitmap.hpp>

#include <rendering/ShaderManager.hpp>

#include <rendering/RenderObject.hpp>

#include <Constants.hpp>

namespace hyperion {

class Engine;
class Texture;
class Mesh;
struct RenderSetup;

struct alignas(16) GaussianSplattingPoint
{
    alignas(16) Vector4 position;
    alignas(16) Quaternion rotation;
    alignas(16) Vector4 scale;
    alignas(16) Vector4 color;
};

static_assert(sizeof(GaussianSplattingPoint) == 64);

struct GaussianSplattingModelData
{
    Array<GaussianSplattingPoint> points;
    Transform transform;
};

HYP_CLASS()
class HYP_API GaussianSplattingInstance final : public HypObjectBase
{
    HYP_OBJECT_BODY(GaussianSplattingInstance);

public:
    enum SortStage
    {
        SORT_STAGE_FIRST,
        SORT_STAGE_SECOND,
        SORT_STAGE_MAX
    };

    GaussianSplattingInstance();
    GaussianSplattingInstance(RC<GaussianSplattingModelData> model);
    GaussianSplattingInstance(const GaussianSplattingInstance& other) = delete;
    GaussianSplattingInstance& operator=(const GaussianSplattingInstance& other) = delete;
    ~GaussianSplattingInstance();

    const RC<GaussianSplattingModelData>& GetModel() const
    {
        return m_model;
    }

    const GpuBufferRef& GetSplatBuffer() const
    {
        return m_splatBuffer;
    }

    const GpuBufferRef& GetIndirectBuffer() const
    {
        return m_indirectBuffer;
    }

    const GraphicsPipelineRef& GetGraphicsPipeline() const
    {
        return m_graphicsPipeline;
    }

    const ComputePipelineRef& GetUpdateSplatsComputePipeline() const
    {
        return m_updateSplats;
    }

    const ComputePipelineRef& GetSortSplatsComputePipeline() const
    {
        return m_sortSplats;
    }

    void Record(FrameBase* frame, const RenderSetup& renderSetup);

private:
    void Init() override;

    void CreateBuffers();
    void CreateShader();
    void CreateGraphicsPipeline();
    void CreateComputePipelines();

    RC<GaussianSplattingModelData> m_model;
    GpuBufferRef m_splatBuffer;
    GpuBufferRef m_splatIndicesBuffer;
    GpuBufferRef m_sceneBuffer;
    GpuBufferRef m_indirectBuffer;
    ComputePipelineRef m_updateSplats;
    ComputePipelineRef m_updateSplatDistances;
    ComputePipelineRef m_sortSplats;
    ComputePipelineRef m_sortSplatsTranspose;
    Array<DescriptorTableRef> m_sortStageDescriptorTables;
    ShaderRef m_shader;
    GraphicsPipelineRef m_graphicsPipeline;

    // inefficient cpu-based sort, just to test
    Array<uint32> m_cpuSortedIndices;
    Array<float32> m_cpuDistances;
};

HYP_CLASS()
class HYP_API GaussianSplatting final : public HypObjectBase
{
    HYP_OBJECT_BODY(GaussianSplatting);

public:
    GaussianSplatting();
    GaussianSplatting(const GaussianSplatting& other) = delete;
    GaussianSplatting& operator=(const GaussianSplatting& other) = delete;
    ~GaussianSplatting();

    const Handle<GaussianSplattingInstance>& GetGaussianSplattingInstance() const
    {
        return m_gaussianSplattingInstance;
    }

    void SetGaussianSplattingInstance(Handle<GaussianSplattingInstance> gaussianSplattingInstance);

    void UpdateSplats(FrameBase* frame, const RenderSetup& renderSetup);

    void Render(FrameBase* frame, const RenderSetup& renderSetup);

private:
    void Init() override;
    void CreateBuffers();

    Handle<Mesh> m_quadMesh;

    // for zeroing out data
    GpuBufferRef m_stagingBuffer;

    Handle<GaussianSplattingInstance> m_gaussianSplattingInstance;
};

} // namespace hyperion

