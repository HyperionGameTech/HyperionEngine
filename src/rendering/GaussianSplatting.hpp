/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GAUSSIAN_SPLATTING_HPP
#define HYPERION_GAUSSIAN_SPLATTING_HPP

#include <core/threading/Threads.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>

#include <util/img/Bitmap.hpp>

#include <rendering/Shader.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Constants.hpp>

namespace hyperion {

class Engine;
class Texture;

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
class HYP_API GaussianSplattingInstance : public HypObject<GaussianSplattingInstance>
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

    const GPUBufferRef& GetSplatBuffer() const
    {
        return m_splat_buffer;
    }

    const GPUBufferRef& GetIndirectBuffer() const
    {
        return m_indirect_buffer;
    }

    const Handle<RenderGroup>& GetRenderGroup() const
    {
        return m_render_group;
    }

    const ComputePipelineRef& GetUpdateSplatsComputePipeline() const
    {
        return m_update_splats;
    }

    const ComputePipelineRef& GetSortSplatsComputePipeline() const
    {
        return m_sort_splats;
    }

    void Init();
    void Record(FrameBase* frame);

private:
    void CreateBuffers();
    void CreateShader();
    void CreateRenderGroup();
    void CreateComputePipelines();

    RC<GaussianSplattingModelData> m_model;
    GPUBufferRef m_splat_buffer;
    GPUBufferRef m_splat_indices_buffer;
    GPUBufferRef m_scene_buffer;
    GPUBufferRef m_indirect_buffer;
    ComputePipelineRef m_update_splats;
    ComputePipelineRef m_update_splat_distances;
    ComputePipelineRef m_sort_splats;
    ComputePipelineRef m_sort_splats_transpose;
    Array<DescriptorTableRef> m_sort_stage_descriptor_tables;
    ShaderRef m_shader;
    Handle<RenderGroup> m_render_group;

    // inefficient cpu-based sort, just to test
    Array<uint32> m_cpu_sorted_indices;
    Array<float32> m_cpu_distances;
};

HYP_CLASS()
class HYP_API GaussianSplatting : public HypObject<GaussianSplatting>
{
    HYP_OBJECT_BODY(GaussianSplatting);

public:
    GaussianSplatting();
    GaussianSplatting(const GaussianSplatting& other) = delete;
    GaussianSplatting& operator=(const GaussianSplatting& other) = delete;
    ~GaussianSplatting();

    const Handle<GaussianSplattingInstance>& GetGaussianSplattingInstance() const
    {
        return m_gaussian_splatting_instance;
    }

    void SetGaussianSplattingInstance(Handle<GaussianSplattingInstance> gaussian_splatting_instance);

    void Init();

    void UpdateSplats(FrameBase* frame);

    void Render(FrameBase* frame);

private:
    void CreateBuffers();

    Handle<Mesh> m_quad_mesh;

    // for zeroing out data
    GPUBufferRef m_staging_buffer;

    Handle<GaussianSplattingInstance> m_gaussian_splatting_instance;
};

} // namespace hyperion

#endif