/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/Lightmapper.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <scene/BVH.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Texture.hpp>
#include <scene/View.hpp>

#include <scene/lightmapper/LightmapVolume.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/LightmapVolumeComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/BVHComponent.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/Time.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/math/Triangle.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <system/AppContext.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)
    : renderer::RenderCommand
{
    GPUBufferRef uniform_buffer;

    RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)(GPUBufferRef uniform_buffer)
        : uniform_buffer(std::move(uniform_buffer))
    {
    }

    virtual ~RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create());
        uniform_buffer->Memset(sizeof(RTRadianceUniforms), 0x0);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(LightmapRender)
    : renderer::RenderCommand
{
    LightmapJob* job;
    RenderView* view;
    Array<LightmapRay> rays;
    uint32 ray_offset;

    RENDER_COMMAND(LightmapRender)(LightmapJob* job, RenderView* view, Array<LightmapRay>&& rays, uint32 ray_offset)
        : job(job),
          view(view),
          rays(std::move(rays)),
          ray_offset(ray_offset)
    {
        if (view)
        {
            view->IncRef();
        }

        job->m_num_concurrent_rendering_tasks.Increment(1, MemoryOrder::RELEASE);
    }

    virtual ~RENDER_COMMAND(LightmapRender)() override
    {
        if (view)
        {
            view->DecRef();
        }

        job->m_num_concurrent_rendering_tasks.Decrement(1, MemoryOrder::RELEASE);
    }

    virtual RendererResult operator()() override
    {
        FrameBase* frame = g_rendering_api->GetCurrentFrame();

        RenderSetup render_setup { &g_engine->GetWorld()->GetRenderResource(), view };

        if (view)
        {
            if (const Array<RenderEnvProbe*>& sky_env_probes = view->GetEnvProbes(EnvProbeType::SKY); sky_env_probes.Any())
            {
                render_setup.env_probe = sky_env_probes[0];
            }
        }

        const uint32 frame_index = frame->GetFrameIndex();
        const uint32 previous_frame_index = (frame_index + max_frames_in_flight - 1) % max_frames_in_flight;

        {
            // Read ray hits from last time this frame was rendered
            Array<LightmapRay> previous_rays;
            job->GetPreviousFrameRays(previous_rays);

            // Read previous frame hits into CPU buffer
            if (previous_rays.Size() != 0)
            {
                Array<LightmapHit> hits_buffer;
                hits_buffer.Resize(previous_rays.Size());

                for (ILightmapRenderer* lightmap_renderer : job->GetParams().renderers)
                {
                    AssertThrow(lightmap_renderer != nullptr);

                    lightmap_renderer->ReadHitsBuffer(frame, hits_buffer);

                    job->IntegrateRayHits(previous_rays, hits_buffer, lightmap_renderer->GetShadingType());
                }
            }

            job->SetPreviousFrameRays(rays);
        }

        if (rays.Any())
        {
            for (ILightmapRenderer* lightmap_renderer : job->GetParams().renderers)
            {
                AssertThrow(lightmap_renderer != nullptr);

                lightmap_renderer->Render(frame, render_setup, job, rays, ray_offset);
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region LightmapperConfig

void LightmapperConfig::PostLoadCallback()
{
    if (trace_mode == LightmapTraceMode::GPU_PATH_TRACING)
    {
        if (!g_rendering_api->GetRenderConfig().IsRaytracingSupported())
        {
            trace_mode = LightmapTraceMode::CPU_PATH_TRACING;

            HYP_LOG(Lightmap, Warning, "GPU path tracing is not supported on this device. Falling back to CPU path tracing.");
        }
    }
}

#pragma endregion LightmapperConfig

#pragma region LightmapAccelerationStructure

struct LightmapRayHit : RayHit
{
    Handle<Entity> entity;
    Triangle triangle;

    LightmapRayHit() = default;

    LightmapRayHit(const RayHit& ray_hit, const Handle<Entity>& entity, const Triangle& triangle)
        : RayHit(ray_hit),
          entity(entity),
          triangle(triangle)
    {
    }

    LightmapRayHit(const LightmapRayHit& other) = default;
    LightmapRayHit& operator=(const LightmapRayHit& other) = default;

    LightmapRayHit(LightmapRayHit&& other) noexcept
        : RayHit(static_cast<RayHit&&>(std::move(other))),
          entity(std::move(other.entity)),
          triangle(std::move(other.triangle))
    {
    }

    LightmapRayHit& operator=(LightmapRayHit&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        RayHit::operator=(static_cast<RayHit&&>(std::move(other)));
        entity = std::move(other.entity);
        triangle = std::move(other.triangle);

        return *this;
    }

    virtual ~LightmapRayHit() = default;

    bool operator==(const LightmapRayHit& other) const
    {
        return static_cast<const RayHit&>(*this) == static_cast<const RayHit&>(other)
            && entity == other.entity
            && triangle == other.triangle;
    }

    bool operator!=(const LightmapRayHit& other) const
    {
        return static_cast<const RayHit&>(*this) != static_cast<const RayHit&>(other)
            || entity != other.entity
            || triangle != other.triangle;
    }

    bool operator<(const LightmapRayHit& other) const
    {
        if (static_cast<const RayHit&>(*this) < static_cast<const RayHit&>(other))
        {
            return true;
        }

        if (entity < other.entity)
        {
            return true;
        }

        if (entity == other.entity && triangle.GetPosition() < other.triangle.GetPosition())
        {
            return true;
        }

        return false;
    }
};

using LightmapRayTestResults = FlatSet<LightmapRayHit>;

class ILightmapAccelerationStructure
{
public:
    virtual ~ILightmapAccelerationStructure() = default;

    virtual const Transform& GetTransform() const = 0;

    virtual LightmapRayTestResults TestRay(const Ray& ray) const = 0;
};

class LightmapBottomLevelAccelerationStructure final : public ILightmapAccelerationStructure
{
public:
    LightmapBottomLevelAccelerationStructure(const LightmapSubElement* sub_element, const BVHNode* bvh)
        : m_sub_element(sub_element),
          m_root(bvh)
    {
        AssertThrow(m_sub_element != nullptr);
        AssertThrow(m_root != nullptr);
    }

    LightmapBottomLevelAccelerationStructure(const LightmapBottomLevelAccelerationStructure& other) = delete;
    LightmapBottomLevelAccelerationStructure& operator=(const LightmapBottomLevelAccelerationStructure& other) = delete;

    LightmapBottomLevelAccelerationStructure(LightmapBottomLevelAccelerationStructure&& other) noexcept
        : m_sub_element(other.m_sub_element),
          m_root(other.m_root)
    {
        other.m_sub_element = nullptr;
        other.m_root = nullptr;
    }

    LightmapBottomLevelAccelerationStructure& operator=(LightmapBottomLevelAccelerationStructure&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_sub_element = other.m_sub_element;
        m_root = std::move(other.m_root);

        other.m_sub_element = nullptr;
        other.m_root = nullptr;

        return *this;
    }

    virtual ~LightmapBottomLevelAccelerationStructure() override = default;

    HYP_FORCE_INLINE const Handle<Entity>& GetEntity() const
    {
        return m_sub_element->entity;
    }

    virtual const Transform& GetTransform() const override
    {
        return m_sub_element->transform;
    }

    virtual LightmapRayTestResults TestRay(const Ray& ray) const override
    {
        AssertThrow(m_root != nullptr);

        LightmapRayTestResults results;

        const Matrix4& model_matrix = m_sub_element->transform.GetMatrix();

        const Ray local_space_ray = model_matrix.Inverted() * ray;

        RayTestResults local_bvh_results = m_root->TestRay(local_space_ray);

        if (local_bvh_results.Any())
        {
            const Matrix4 normal_matrix = model_matrix.Transposed().Inverted();

            RayTestResults bvh_results;

            for (RayHit hit : local_bvh_results)
            {
                Vec4f transformed_normal = normal_matrix * Vec4f(hit.normal, 0.0f);
                hit.normal = transformed_normal.GetXYZ().Normalized();

                Vec4f transformed_position = model_matrix * Vec4f(hit.hitpoint, 1.0f);
                transformed_position /= transformed_position.w;

                hit.hitpoint = transformed_position.GetXYZ();

                hit.distance = (hit.hitpoint - ray.position).Length();

                bvh_results.AddHit(hit);
            }

            for (const RayHit& ray_hit : bvh_results)
            {
                AssertThrow(ray_hit.user_data != nullptr);

                const BVHNode* bvh_node = static_cast<const BVHNode*>(ray_hit.user_data);

                const Triangle& triangle = bvh_node->GetTriangles()[ray_hit.id];

                results.Emplace(ray_hit, m_sub_element->entity, triangle);
            }
        }

        return results;
    }

    HYP_FORCE_INLINE const BVHNode* GetRoot() const
    {
        return m_root;
    }

private:
    const LightmapSubElement* m_sub_element;
    const BVHNode* m_root;
};

class LightmapTopLevelAccelerationStructure final : public ILightmapAccelerationStructure
{
public:
    virtual ~LightmapTopLevelAccelerationStructure() override = default;

    virtual const Transform& GetTransform() const override
    {
        return Transform::identity;
    }

    virtual LightmapRayTestResults TestRay(const Ray& ray) const override
    {
        LightmapRayTestResults results;

        for (const LightmapBottomLevelAccelerationStructure& acceleration_structure : m_acceleration_structures)
        {
            if (!ray.TestAABB(acceleration_structure.GetTransform() * acceleration_structure.GetRoot()->GetAABB()))
            {
                continue;
            }

            results.Merge(acceleration_structure.TestRay(ray));
        }

        return results;
    }

    void Add(const LightmapSubElement* sub_element, const BVHNode* bvh)
    {
        m_acceleration_structures.EmplaceBack(sub_element, bvh);
    }

    void RemoveAll()
    {
        m_acceleration_structures.Clear();
    }

private:
    Array<LightmapBottomLevelAccelerationStructure> m_acceleration_structures;
};

#pragma endregion LightmapAccelerationStructure

#pragma region LightmapperWorkerThread

class LightmapperWorkerThread : public TaskThread
{
public:
    LightmapperWorkerThread(ThreadID id)
        : TaskThread(id)
    {
    }

    virtual ~LightmapperWorkerThread() override = default;
};

#pragma endregion LightmapperWorkerThread

#pragma region LightmapThreadPool

class LightmapThreadPool : public TaskThreadPool
{
public:
    LightmapThreadPool()
        : TaskThreadPool(TypeWrapper<LightmapperWorkerThread>(), "LightmapperWorker", NumThreadsToCreate())
    {
        HYP_LOG(Lightmap, Info, "Tracing lightmap rays using {} threads", m_threads.Size());
    }

    virtual ~LightmapThreadPool() override = default;

private:
    static uint32 NumThreadsToCreate()
    {
        uint32 num_threads = g_engine->GetAppContext()->GetConfiguration().Get("lightmapper.num_threads_per_job").ToUInt32(4);
        return MathUtil::Clamp(num_threads, 1u, 128u);
    }
};

#pragma endregion LightmapThreadPool

#pragma region LightmapGPUPathTracer

class HYP_API LightmapGPUPathTracer : public ILightmapRenderer
{
public:
    LightmapGPUPathTracer(const Handle<Scene>& scene, LightmapShadingType shading_type);
    LightmapGPUPathTracer(const LightmapGPUPathTracer& other) = delete;
    LightmapGPUPathTracer& operator=(const LightmapGPUPathTracer& other) = delete;
    LightmapGPUPathTracer(LightmapGPUPathTracer&& other) noexcept = delete;
    LightmapGPUPathTracer& operator=(LightmapGPUPathTracer&& other) noexcept = delete;
    virtual ~LightmapGPUPathTracer() override;

    HYP_FORCE_INLINE const RaytracingPipelineRef& GetPipeline() const
    {
        return m_raytracing_pipeline;
    }

    virtual uint32 MaxRaysPerFrame() const override
    {
        return uint32(-1);
    }

    virtual LightmapShadingType GetShadingType() const override
    {
        return m_shading_type;
    }

    virtual void Create() override;
    virtual void UpdateRays(Span<const LightmapRay> rays) override;
    virtual void ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> out_hits) override;
    virtual void Render(FrameBase* frame, const RenderSetup& render_setup, LightmapJob* job, Span<const LightmapRay> rays, uint32 ray_offset) override;

private:
    void CreateUniformBuffer();
    void UpdateUniforms(FrameBase* frame, uint32 ray_offset);

    Handle<Scene> m_scene;
    LightmapShadingType m_shading_type;

    FixedArray<GPUBufferRef, max_frames_in_flight> m_uniform_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight> m_rays_buffers;

    GPUBufferRef m_hits_buffer_gpu;

    RaytracingPipelineRef m_raytracing_pipeline;
};

LightmapGPUPathTracer::LightmapGPUPathTracer(const Handle<Scene>& scene, LightmapShadingType shading_type)
    : m_scene(scene),
      m_shading_type(shading_type),
      m_uniform_buffers({ g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::CONSTANT_BUFFER, sizeof(RTRadianceUniforms)),
          g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::CONSTANT_BUFFER, sizeof(RTRadianceUniforms)) }),
      m_rays_buffers({ g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::STORAGE_BUFFER, sizeof(Vec4f) * 2 * (512 * 512)),
          g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::STORAGE_BUFFER, sizeof(Vec4f) * 2 * (512 * 512)) }),
      m_hits_buffer_gpu(g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::STORAGE_BUFFER, sizeof(LightmapHit) * (512 * 512)))
{
}

LightmapGPUPathTracer::~LightmapGPUPathTracer()
{
    SafeRelease(std::move(m_uniform_buffers));
    SafeRelease(std::move(m_rays_buffers));
    SafeRelease(std::move(m_hits_buffer_gpu));
    SafeRelease(std::move(m_raytracing_pipeline));
}

void LightmapGPUPathTracer::CreateUniformBuffer()
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        m_uniform_buffers[frame_index] = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(RTRadianceUniforms));

        PUSH_RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer, m_uniform_buffers[frame_index]);
    }
}

void LightmapGPUPathTracer::Create()
{
    AssertThrow(m_scene.IsValid());

    AssertThrow(m_scene->GetWorld() != nullptr);
    AssertThrow(m_scene->GetWorld()->IsReady());

    CreateUniformBuffer();

    DeferCreate(m_hits_buffer_gpu);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        DeferCreate(m_rays_buffers[frame_index]);
    }

    ShaderProperties shader_properties;

    switch (m_shading_type)
    {
    case LightmapShadingType::RADIANCE:
        shader_properties.Set("MODE_RADIANCE");
        break;
    case LightmapShadingType::IRRADIANCE:
        shader_properties.Set("MODE_IRRADIANCE");
        break;
    default:
        HYP_UNREACHABLE();
    }

    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("LightmapGPUPathTracer"), shader_properties);
    AssertThrow(shader.IsValid());

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const TLASRef& tlas = m_scene->GetWorld()->GetRenderResource().GetEnvironment()->GetTopLevelAccelerationStructures()[frame_index];
        AssertThrow(tlas != nullptr);

        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("TLAS"), tlas);
        descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), tlas->GetMeshDescriptionsBuffer());
        descriptor_set->SetElement(NAME("HitsBuffer"), m_hits_buffer_gpu);
        descriptor_set->SetElement(NAME("RaysBuffer"), m_rays_buffers[frame_index]);

        descriptor_set->SetElement(NAME("LightsBuffer"), g_render_global_state->Lights->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("MaterialsBuffer"), g_render_global_state->Materials->GetBuffer(frame_index));

        descriptor_set->SetElement(NAME("RTRadianceUniforms"), m_uniform_buffers[frame_index]);
    }

    DeferCreate(descriptor_table);

    m_raytracing_pipeline = g_rendering_api->MakeRaytracingPipeline(
        shader,
        descriptor_table);

    DeferCreate(m_raytracing_pipeline);
}

void LightmapGPUPathTracer::UpdateUniforms(FrameBase* frame, uint32 ray_offset)
{
    RTRadianceUniforms uniforms {};
    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    uniforms.ray_offset = ray_offset;

    // const uint32 max_bound_lights = MathUtil::Min(g_engine->GetRenderState()->NumBoundLights(), ArraySize(uniforms.light_indices));
    // uint32 num_bound_lights = 0;

    // for (uint32 light_type = 0; light_type < uint32(LightType::MAX); light_type++) {
    //     if (num_bound_lights >= max_bound_lights) {
    //         break;
    //     }

    //     for (const auto &it : g_engine->GetRenderState()->bound_lights[light_type]) {
    //         if (num_bound_lights >= max_bound_lights) {
    //             break;
    //         }

    //         uniforms.light_indices[num_bound_lights++] = it->GetBufferIndex();
    //     }
    // }

    // uniforms.num_bound_lights = num_bound_lights;

    // FIXME: Lights are now stored per-view.
    // We don't have a View for Lightmapper since it is for the entire RenderWorld it is indirectly attached to.
    // We'll need to find a way to get the lights for the current view.
    // Ideas:
    // a) create a View for the Lightmapper and use that to get the lights. It will need to collect the lights on the Game thread so we'll need to add some kind of System to do that.
    // b) add a function to the RenderScene to get all the lights in the scene and use that to get the lights for the current view. This has a drawback that we will always have some RenderLight active when it could be inactive if it is not in any view.
    // OR: We can just use the lights in the current view and ignore the rest. This is a bit of a hack but it will work for now.
    HYP_NOT_IMPLEMENTED();

    uniforms.num_bound_lights = 0;

    m_uniform_buffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);
}

void LightmapGPUPathTracer::UpdateRays(Span<const LightmapRay> rays)
{
}

void LightmapGPUPathTracer::ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> out_hits)
{
    // @TODO Some kind of function like WaitForFrameToComplete to ensure that the hits buffer is not being written to in the current frame.

    const GPUBufferRef& hits_buffer = m_hits_buffer_gpu;

    GPUBufferRef staging_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STAGING_BUFFER, out_hits.Size() * sizeof(LightmapHit));
    HYPERION_ASSERT_RESULT(staging_buffer->Create());
    staging_buffer->Memset(out_hits.Size() * sizeof(LightmapHit), 0);

    renderer::SingleTimeCommands commands;

    commands.Push([&](RHICommandList& cmd)
        {
            const renderer::ResourceState previous_resource_state = hits_buffer->GetResourceState();

            // put src image in state for copying from
            cmd.Add<InsertBarrier>(hits_buffer, renderer::ResourceState::COPY_SRC);

            // put dst buffer in state for copying to
            cmd.Add<InsertBarrier>(staging_buffer, renderer::ResourceState::COPY_DST);

            cmd.Add<CopyBuffer>(staging_buffer, hits_buffer, out_hits.Size() * sizeof(LightmapHit));

            cmd.Add<InsertBarrier>(staging_buffer, renderer::ResourceState::COPY_SRC);

            cmd.Add<InsertBarrier>(hits_buffer, previous_resource_state);
        });

    HYPERION_ASSERT_RESULT(commands.Execute());

    staging_buffer->Read(sizeof(LightmapHit) * out_hits.Size(), out_hits.Data());

    HYPERION_ASSERT_RESULT(staging_buffer->Destroy());
}

void LightmapGPUPathTracer::Render(FrameBase* frame, const RenderSetup& render_setup, LightmapJob* job, Span<const LightmapRay> rays, uint32 ray_offset)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_setup.IsValid());

    const uint32 frame_index = frame->GetFrameIndex();
    const uint32 previous_frame_index = (frame->GetFrameIndex() + max_frames_in_flight - 1) % max_frames_in_flight;

    UpdateUniforms(frame, ray_offset);

    { // rays buffer
        Array<Vec4f> ray_data;
        ray_data.Resize(rays.Size() * 2);

        for (SizeType i = 0; i < rays.Size(); i++)
        {
            ray_data[i * 2] = Vec4f(rays[i].ray.position, 1.0f);
            ray_data[i * 2 + 1] = Vec4f(rays[i].ray.direction, 0.0f);
        }

        bool rays_buffer_resized = false;

        HYPERION_ASSERT_RESULT(m_rays_buffers[frame->GetFrameIndex()]->EnsureCapacity(ray_data.ByteSize(), &rays_buffer_resized));
        m_rays_buffers[frame->GetFrameIndex()]->Copy(ray_data.ByteSize(), ray_data.Data());

        if (rays_buffer_resized)
        {
            m_raytracing_pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex())->SetElement(NAME("RaysBuffer"), m_rays_buffers[frame->GetFrameIndex()]);
        }

        bool hits_buffer_resized = false;

        /*HYPERION_ASSERT_RESULT(m_hits_buffers[frame->GetFrameIndex()]->EnsureCapacity(rays.Size() * sizeof(LightmapHit), &hits_buffer_resized));
        m_hits_buffers[frame->GetFrameIndex()]->Memset(rays.Size() * sizeof(LightmapHit), 0);

        if (hits_buffer_resized) {
            m_raytracing_pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex())
                ->SetElement(NAME("HitsBuffer"), m_hits_buffers[frame->GetFrameIndex()]);
        }*/

        if (rays_buffer_resized || hits_buffer_resized)
        {
            m_raytracing_pipeline->GetDescriptorTable()->Update(frame->GetFrameIndex());
        }
    }

    frame->GetCommandList().Add<BindRaytracingPipeline>(m_raytracing_pipeline);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_raytracing_pipeline->GetDescriptorTable(),
        m_raytracing_pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(render_setup.env_grid, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<InsertBarrier>(m_hits_buffer_gpu, renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandList().Add<TraceRays>(
        m_raytracing_pipeline,
        Vec3u { uint32(rays.Size()), 1, 1 });

    frame->GetCommandList().Add<InsertBarrier>(m_hits_buffer_gpu, renderer::ResourceState::UNORDERED_ACCESS);
}

#pragma endregion LightmapGPUPathTracer

#pragma region LightmapCPUPathTracer

class HYP_API LightmapCPUPathTracer : public ILightmapRenderer
{
public:
    LightmapCPUPathTracer(const Handle<Scene>& scene, LightmapShadingType shading_type);
    LightmapCPUPathTracer(const LightmapCPUPathTracer& other) = delete;
    LightmapCPUPathTracer& operator=(const LightmapCPUPathTracer& other) = delete;
    LightmapCPUPathTracer(LightmapCPUPathTracer&& other) noexcept = delete;
    LightmapCPUPathTracer& operator=(LightmapCPUPathTracer&& other) noexcept = delete;
    virtual ~LightmapCPUPathTracer() override;

    virtual uint32 MaxRaysPerFrame() const override
    {
        return uint32(-1);
    }

    virtual LightmapShadingType GetShadingType() const override
    {
        return m_shading_type;
    }

    virtual void Create() override;
    virtual void UpdateRays(Span<const LightmapRay> rays) override;
    virtual void ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> out_hits) override;
    virtual void Render(FrameBase* frame, const RenderSetup& render_setup, LightmapJob* job, Span<const LightmapRay> rays, uint32 ray_offset) override;

private:
    void TraceSingleRayOnCPU(LightmapJob* job, const LightmapRay& ray, LightmapRayHitPayload& out_payload);

    Handle<Scene> m_scene;
    LightmapShadingType m_shading_type;

    Array<LightmapHit, DynamicAllocator> m_hits_buffer;

    Array<LightmapRay, DynamicAllocator> m_current_rays;

    LightmapThreadPool m_thread_pool;

    AtomicVar<uint32> m_num_tracing_tasks;
};

LightmapCPUPathTracer::LightmapCPUPathTracer(const Handle<Scene>& scene, LightmapShadingType shading_type)
    : m_scene(scene),
      m_shading_type(shading_type),
      m_num_tracing_tasks(0)
{
}

LightmapCPUPathTracer::~LightmapCPUPathTracer()
{
    if (m_thread_pool.IsRunning())
    {
        m_thread_pool.Stop();
    }
}

void LightmapCPUPathTracer::Create()
{
    m_thread_pool.Start();
}

void LightmapCPUPathTracer::UpdateRays(Span<const LightmapRay> rays)
{
}

void LightmapCPUPathTracer::ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> out_hits)
{
    Threads::AssertOnThread(g_render_thread);

    AssertThrowMsg(m_num_tracing_tasks.Get(MemoryOrder::ACQUIRE) == 0,
        "Cannot read hits buffer while tracing is in progress");

    AssertThrow(out_hits.Size() == m_hits_buffer.Size());

    Memory::MemCpy(out_hits.Data(), m_hits_buffer.Data(), m_hits_buffer.ByteSize());
}

void LightmapCPUPathTracer::Render(FrameBase* frame, const RenderSetup& render_setup, LightmapJob* job, Span<const LightmapRay> rays, uint32 ray_offset)
{
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_setup.IsValid());

    AssertThrowMsg(m_num_tracing_tasks.Get(MemoryOrder::ACQUIRE) == 0,
        "Trace is already in progress");

    Handle<Texture> env_probe_texture;

    if (render_setup.env_probe)
    {
        // prepare env probe texture to be sampled on the CPU in the tasks
        render_setup.env_probe->GetPrefilteredEnvMap()->Readback();
        env_probe_texture = render_setup.env_probe->GetPrefilteredEnvMap();
    }

    m_hits_buffer.Resize(rays.Size());

    m_current_rays.Resize(rays.Size());
    Memory::MemCpy(m_current_rays.Data(), rays.Data(), m_current_rays.ByteSize());

    m_num_tracing_tasks.Increment(rays.Size(), MemoryOrder::RELEASE);

    TaskBatch* task_batch = new TaskBatch();
    task_batch->pool = &m_thread_pool;

    const uint32 num_items = uint32(m_current_rays.Size());
    const uint32 num_batches = m_thread_pool.GetProcessorAffinity();
    const uint32 items_per_batch = (num_items + num_batches - 1) / num_batches;

    for (uint32 batch_index = 0; batch_index < num_batches; batch_index++)
    {
        task_batch->AddTask([this, job, batch_index, items_per_batch, num_items, env_probe_texture = env_probe_texture](...)
            {
                const uint32 offset_index = batch_index * items_per_batch;
                const uint32 max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

                for (uint32 index = offset_index; index < max_index; index++)
                {
                    HYP_DEFER({ m_num_tracing_tasks.Decrement(1, MemoryOrder::RELEASE); });

                    const LightmapRay& first_ray = m_current_rays[index];

                    uint32 seed = (uint32)rand(); // index * m_texel_index;

                    FixedArray<LightmapRay, max_bounces_cpu + 1> recursive_rays;
                    FixedArray<LightmapRayHitPayload, max_bounces_cpu + 1> bounces;

                    int num_bounces = 0;

                    Vec3f direction = first_ray.ray.direction.Normalized();

                    if (m_shading_type == LightmapShadingType::IRRADIANCE)
                    {
                        direction = MathUtil::RandomInHemisphere(
                            Vec3f(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed)),
                            first_ray.ray.direction)
                                        .Normalize();
                    }

                    Vec3f origin = first_ray.ray.position + direction * 0.001f;

                    // Vec4f sky_color;

                    // if (env_probe_texture.IsValid()) {
                    //     Vec4f env_probe_sample = env_probe_texture->SampleCube(direction);

                    //     sky_color = env_probe_sample;
                    // }

                    for (int bounce_index = 0; bounce_index < max_bounces_cpu; bounce_index++)
                    {
                        LightmapRay bounce_ray = first_ray;

                        if (bounce_index != 0)
                        {
                            bounce_ray.mesh_id = bounces[bounce_index - 1].mesh_id;
                            bounce_ray.triangle_index = bounces[bounce_index - 1].triangle_index;
                        }

                        bounce_ray.ray = Ray {
                            origin,
                            direction
                        };

                        recursive_rays[bounce_index] = bounce_ray;

                        LightmapRayHitPayload& payload = bounces[bounce_index];
                        payload = {};

                        TraceSingleRayOnCPU(job, bounce_ray, payload);

                        if (payload.distance < 0.0f)
                        {
                            payload.throughput = Vec4f(0.0f);

                            AssertThrow(bounce_index < bounces.Size());

                            // @TODO Sample environment map
                            const Vec3f normal = bounce_ray.ray.direction;

                            // if (env_probe_texture.IsValid()) {
                            //     Vec4f env_probe_sample = env_probe_texture->SampleCube(direction);

                            //     payload.emissive += env_probe_sample;
                            // }

                            // testing!! @FIXME
                            const Vec3f L = Vec3f(-0.4f, 0.65f, 0.1f).Normalize();
                            payload.emissive += Vec4f(1.0f) * MathUtil::Max(0.0f, normal.Dot(L));

                            ++num_bounces;

                            break;
                        }

                        Vec3f hit_position = origin + direction * payload.distance;

                        if (m_shading_type == LightmapShadingType::IRRADIANCE)
                        {
                            const Vec3f rnd = Vec3f(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed));

                            direction = MathUtil::RandomInHemisphere(rnd, payload.normal).Normalize();
                        }
                        else
                        {
                            ++num_bounces;
                            break;
                        }

                        origin = hit_position + direction * 0.001f;

                        ++num_bounces;
                    }

                    for (int bounce_index = int(num_bounces - 1); bounce_index >= 0; bounce_index--)
                    {
                        Vec4f radiance = bounces[bounce_index].emissive;

                        if (bounce_index != num_bounces - 1)
                        {
                            radiance += bounces[bounce_index + 1].radiance * bounces[bounce_index].throughput;
                        }

                        float p = MathUtil::Max(radiance.x, MathUtil::Max(radiance.y, MathUtil::Max(radiance.z, radiance.w)));

                        if (MathUtil::RandomFloat(seed) > p)
                        {
                            break;
                        }

                        radiance /= MathUtil::Max(p, 0.0001f);

                        bounces[bounce_index].radiance = radiance;
                    }

                    LightmapHit& hit = m_hits_buffer[index];

                    if (num_bounces != 0)
                    {
                        hit.color = bounces[0].radiance;

                        if (MathUtil::IsNaN(hit.color) || !MathUtil::IsFinite(hit.color))
                        {
                            HYP_LOG_ONCE(Lightmap, Warning, "NaN or infinite color detected while tracing rays");

                            hit.color = Vec4f(0.0f);
                        }

                        hit.color.w = 1.0f;
                    }
                }
            });
    }

    TaskSystem::GetInstance().EnqueueBatch(task_batch);

    job->AddTask(task_batch);
}

void LightmapCPUPathTracer::TraceSingleRayOnCPU(LightmapJob* job, const LightmapRay& ray, LightmapRayHitPayload& out_payload)
{
    out_payload.throughput = Vec4f(0.0f);
    out_payload.emissive = Vec4f(0.0f);
    out_payload.radiance = Vec4f(0.0f);
    out_payload.normal = Vec3f(0.0f);
    out_payload.distance = -1.0f;
    out_payload.barycentric_coords = Vec3f(0.0f);
    out_payload.mesh_id = ID<Mesh>::invalid;
    out_payload.triangle_index = ~0u;

    ILightmapAccelerationStructure* acceleration_structure = job->GetParams().acceleration_structure;

    if (!acceleration_structure)
    {
        HYP_LOG(Lightmap, Warning, "No acceleration structure set while tracing on CPU, cannot perform trace");

        return;
    }

    LightmapRayTestResults results = acceleration_structure->TestRay(ray.ray);

    if (!results.Any())
    {
        return;
    }

    for (const LightmapRayHit& hit : results)
    {
        if (hit.distance + 0.0001f <= 0.0f)
        {
            continue;
        }

        AssertThrow(hit.entity.IsValid());

        auto it = job->GetParams().sub_elements_by_entity->Find(hit.entity);

        AssertThrow(it != job->GetParams().sub_elements_by_entity->End());
        AssertThrow(it->second != nullptr);

        const LightmapSubElement& sub_element = *it->second;

        const ID<Mesh> mesh_id = sub_element.mesh->GetID();

        const Vec3f barycentric_coords = hit.barycentric_coords;

        const Triangle& triangle = hit.triangle;

        const Vec2f uv = triangle.GetPoint(0).GetTexCoord0() * barycentric_coords.x
            + triangle.GetPoint(1).GetTexCoord0() * barycentric_coords.y
            + triangle.GetPoint(2).GetTexCoord0() * barycentric_coords.z;

        Vec4f albedo = Vec4f(sub_element.material->GetParameter(Material::MATERIAL_KEY_ALBEDO));

        // sample albedo texture, if present
        if (const Handle<Texture>& albedo_texture = sub_element.material->GetTexture(MaterialTextureKey::ALBEDO_MAP))
        {
            Vec4f albedo_texture_color = albedo_texture->Sample2D(uv);

            albedo *= albedo_texture_color;
        }

        out_payload.emissive = Vec4f(0.0f);
        out_payload.throughput = albedo;
        out_payload.barycentric_coords = barycentric_coords;
        out_payload.mesh_id = mesh_id;
        out_payload.triangle_index = hit.id;
        out_payload.normal = hit.normal;
        out_payload.distance = hit.distance;

        return;
    }
}

#pragma endregion LightmapCPUPathTracer

#pragma region LightmapJob

static constexpr uint32 g_max_concurrent_rendering_tasks_per_job = 1;

LightmapJob::LightmapJob(LightmapJobParams&& params)
    : m_params(std::move(params)),
      m_element_index(~0u),
      m_texel_index(0),
      m_last_logged_percentage(0),
      m_num_concurrent_rendering_tasks(0)
{

    Handle<Camera> camera = CreateObject<Camera>();
    camera->SetName(NAME_FMT("LightmapJob_{}_Camera", m_uuid));
    camera->AddCameraController(CreateObject<OrthoCameraController>());
    InitObject(camera);

    m_view = CreateObject<View>(ViewDesc {
        .flags = ViewFlags::COLLECT_STATIC_ENTITIES
            | ViewFlags::SKIP_FRUSTUM_CULLING
            | ViewFlags::SKIP_ENV_GRIDS
            | ViewFlags::SKIP_LIGHTMAP_VOLUMES,
        .viewport = Viewport { .extent = Vec2i::One(), .position = Vec2i::Zero() },
        .scenes = { m_params.scene },
        .camera = camera });

    InitObject(m_view);
}

LightmapJob::~LightmapJob()
{
    for (TaskBatch* task_batch : m_current_tasks)
    {
        task_batch->AwaitCompletion();

        delete task_batch;
    }

    m_resource_cache.Clear();
}

void LightmapJob::Start()
{
    m_running_semaphore.Produce(1, [this](bool)
        {
            if (!m_uv_map.HasValue())
            {
                // No elements to process
                if (!m_params.sub_elements_view)
                {
                    m_uv_map = LightmapUVMap {};

                    return;
                }

                if (m_params.config->trace_mode == LightmapTraceMode::CPU_PATH_TRACING)
                {
                    HYP_LOG(Lightmap, Info, "Lightmap job {}: Preloading sub-element cached resources", m_uuid);

                    for (const LightmapSubElement& sub_element : m_params.sub_elements_view)
                    {
                        if (sub_element.mesh.IsValid() && sub_element.mesh->GetStreamedMeshData())
                        {
                            m_resource_cache.EmplaceBack(*sub_element.mesh->GetStreamedMeshData());
                        }

                        if (sub_element.material.IsValid())
                        {
                            for (const auto& it : sub_element.material->GetTextures())
                            {
                                if (it.second.IsValid() && it.second->GetStreamedTextureData())
                                {
                                    m_resource_cache.EmplaceBack(*it.second->GetStreamedTextureData());
                                }
                            }
                        }
                    }
                }

                HYP_LOG(Lightmap, Info, "Lightmap job {}: Enqueue task to build UV map", m_uuid);

                m_uv_builder = LightmapUVBuilder { { m_params.sub_elements_view } };

                m_build_uv_map_task = TaskSystem::GetInstance().Enqueue([this]() -> TResult<LightmapUVMap>
                    {
                        return m_uv_builder.Build();
                    },
                    TaskThreadPoolName::THREAD_POOL_BACKGROUND);
            }
        });
}

void LightmapJob::Stop()
{
    m_running_semaphore.Release(1);
}

void LightmapJob::Stop(const Error& error)
{
    HYP_LOG(Lightmap, Error, "Lightmap job {} stopped with error: {}", m_uuid, error.GetMessage());

    m_result = error;

    Stop();
}

bool LightmapJob::IsCompleted() const
{
    return !m_running_semaphore.IsInSignalState();
}

void LightmapJob::AddTask(TaskBatch* task_batch)
{
    Mutex::Guard guard(m_current_tasks_mutex);

    m_current_tasks.PushBack(task_batch);
}

void LightmapJob::Process()
{
    AssertThrow(IsRunning());
    AssertThrowMsg(!m_result.HasError(), "Unhandled error in lightmap job: %s", *m_result.GetError().GetMessage());

    if (m_num_concurrent_rendering_tasks.Get(MemoryOrder::ACQUIRE) >= g_max_concurrent_rendering_tasks_per_job)
    {
        // Wait for current rendering tasks to complete before enqueueing new ones.

        return;
    }

    if (!m_uv_map.HasValue())
    {
        // wait for uv map to finish building

        // If uv map is not valid, it must have a task that is building it
        AssertThrow(m_build_uv_map_task.IsValid());

        if (!m_build_uv_map_task.IsCompleted())
        {
            // return early so we don't block - we need to wait for build task to complete before processing
            return;
        }

        if (TResult<LightmapUVMap>& uv_map_result = m_build_uv_map_task.Await(); uv_map_result.HasValue())
        {
            m_uv_map = std::move(*uv_map_result);
        }
        else
        {
            Stop(uv_map_result.GetError());

            return;
        }

        if (m_uv_map.HasValue())
        {
            LightmapElement element;

            if (!m_params.volume->AddElement(*m_uv_map, element, /* shrink_to_fit */ true, /* downscale_limit */ 0.1f))
            {
                Stop(HYP_MAKE_ERROR(Error, "Failed to add LightmapElement to LightmapVolume for lightmap job {}! Dimensions: {}, UV map size: {}",
                    m_uuid, m_params.volume->GetAtlas().atlas_dimensions, Vec2u(m_uv_map->width, m_uv_map->height)));

                return;
            }

            m_element_index = element.index;
            AssertThrow(m_element_index != ~0u);

            // Flatten texel indices, grouped by mesh IDs to prevent unnecessary loading/unloading
            m_texel_indices.Reserve(m_uv_map->uvs.Size());

            for (const auto& it : m_uv_map->mesh_to_uv_indices)
            {
                m_texel_indices.Concat(it.second);
            }

            // Free up memory
            m_uv_map->mesh_to_uv_indices.Clear();
        }
        else
        {
            // Mark as ready to stop further processing
            Stop(HYP_MAKE_ERROR(Error, "Failed to build UV map for lightmap job {}", m_uuid));
        }

        return;
    }

    {
        Mutex::Guard guard(m_current_tasks_mutex);

        if (m_current_tasks.Any())
        {
            for (TaskBatch* task_batch : m_current_tasks)
            {
                if (!task_batch->IsCompleted())
                {
                    // Skip this call

                    return;
                }
            }

            for (TaskBatch* task_batch : m_current_tasks)
            {
                task_batch->AwaitCompletion();

                delete task_batch;
            }

            m_current_tasks.Clear();
        }
    }

    bool has_remaining_rays = false;

    {
        Mutex::Guard guard(m_previous_frame_rays_mutex);

        if (m_previous_frame_rays.Any())
        {
            has_remaining_rays = true;
        }
    }

    if (!has_remaining_rays
        && m_texel_index >= m_texel_indices.Size() * m_params.config->num_samples
        && m_num_concurrent_rendering_tasks.Get(MemoryOrder::ACQUIRE) == 0)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap job {}: All texels processed ({} / {}), stopping", m_uuid, m_texel_index, m_texel_indices.Size() * m_params.config->num_samples);

        Stop();

        return;
    }

    const SizeType max_rays = MathUtil::Min(m_params.renderers[0]->MaxRaysPerFrame(), m_params.config->max_rays_per_frame);

    Array<LightmapRay> rays;
    rays.Reserve(max_rays);

    GatherRays(max_rays, rays);

    const uint32 ray_offset = uint32(m_texel_index % (m_texel_indices.Size() * m_params.config->num_samples));

    for (ILightmapRenderer* lightmap_renderer : m_params.renderers)
    {
        AssertThrow(lightmap_renderer != nullptr);

        lightmap_renderer->UpdateRays(rays);
    }

    const double percentage = double(m_texel_index) / double(m_texel_indices.Size() * m_params.config->num_samples) * 100.0;

    if (MathUtil::Abs(MathUtil::Floor(percentage) - MathUtil::Floor(m_last_logged_percentage)) >= 1)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap job {}: Texel {} / {} ({}%)",
            m_uuid.ToString(), m_texel_index, m_texel_indices.Size() * m_params.config->num_samples, percentage);

        m_last_logged_percentage = percentage;
    }

    PUSH_RENDER_COMMAND(LightmapRender, this, &m_view->GetRenderResource(), std::move(rays), ray_offset);
}

void LightmapJob::GatherRays(uint32 max_ray_hits, Array<LightmapRay>& out_rays)
{
    for (uint32 ray_index = 0; ray_index < max_ray_hits && HasRemainingTexels(); ++ray_index)
    {
        const uint32 texel_index = NextTexel();

        LightmapRay ray = m_uv_map->uvs[texel_index].ray;
        ray.texel_index = texel_index;

        out_rays.PushBack(ray);
    }
}

void LightmapJob::IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shading_type)
{
    AssertThrow(rays.Size() == hits.Size());

    LightmapUVMap& uv_map = GetUVMap();

    for (SizeType i = 0; i < hits.Size(); i++)
    {
        const LightmapRay& ray = rays[i];
        const LightmapHit& hit = hits[i];

        LightmapUV& uv = uv_map.uvs[ray.texel_index];

        switch (shading_type)
        {
        case LightmapShadingType::RADIANCE:
            uv.radiance += Vec4f(hit.color.GetXYZ(), 1.0f); //= Vec4f(MathUtil::Lerp(uv.radiance.GetXYZ() * uv.radiance.w, hit.color.GetXYZ(), hit.color.w), 1.0f);
            break;
        case LightmapShadingType::IRRADIANCE:
            uv.irradiance += Vec4f(hit.color.GetXYZ(), 1.0f); //= Vec4f(MathUtil::Lerp(uv.irradiance.GetXYZ() * uv.irradiance.w, hit.color.GetXYZ(), hit.color.w), 1.0f); //
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
}

#pragma endregion LightmapJob

#pragma region Lightmapper

Lightmapper::Lightmapper(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb)
    : m_config(std::move(config)),
      m_scene(scene),
      m_aabb(aabb),
      m_num_jobs { 0 }
{
    HYP_LOG(Lightmap, Info, "Initializing lightmapper: {}", m_config.ToString());

    for (uint32 i = 0; i < uint32(LightmapShadingType::MAX); i++)
    {
        switch (LightmapShadingType(i))
        {
        case LightmapShadingType::RADIANCE:
            if (!m_config.radiance)
            {
                continue;
            }

            break;
        case LightmapShadingType::IRRADIANCE:
            if (!m_config.irradiance)
            {
                continue;
            }

            break;
        default:
            HYP_UNREACHABLE();
        }

        UniquePtr<ILightmapRenderer>& lightmap_renderer = m_lightmap_renderers.EmplaceBack();

        switch (m_config.trace_mode)
        {
        case LightmapTraceMode::GPU_PATH_TRACING:
            lightmap_renderer.EmplaceAs<LightmapGPUPathTracer>(m_scene, LightmapShadingType(i));
            break;
        case LightmapTraceMode::CPU_PATH_TRACING:
            lightmap_renderer.EmplaceAs<LightmapCPUPathTracer>(m_scene, LightmapShadingType(i));
            break;
        default:
            HYP_UNREACHABLE();
        }

        lightmap_renderer->Create();
    }

    AssertThrow(m_lightmap_renderers.Any());

    m_volume = CreateObject<LightmapVolume>(m_aabb);
    InitObject(m_volume);

    Handle<Entity> lightmap_volume_entity = m_scene->GetEntityManager()->AddEntity();
    m_scene->GetEntityManager()->AddComponent<LightmapVolumeComponent>(lightmap_volume_entity, LightmapVolumeComponent { m_volume });
    m_scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(lightmap_volume_entity, BoundingBoxComponent { m_aabb, m_aabb });

    Handle<Node> lightmap_volume_node = m_scene->GetRoot()->AddChild();
    lightmap_volume_node->SetName(Name::Unique("LightmapVolume"));
    lightmap_volume_node->SetEntity(lightmap_volume_entity);
}

Lightmapper::~Lightmapper()
{
    m_lightmap_renderers = {};

    m_queue.Clear();

    m_acceleration_structure.Reset();
}

bool Lightmapper::IsComplete() const
{
    return m_num_jobs.Get(MemoryOrder::ACQUIRE) == 0;
}

LightmapJobParams Lightmapper::CreateLightmapJobParams(
    SizeType start_index,
    SizeType end_index,
    LightmapTopLevelAccelerationStructure* acceleration_structure)
{
    LightmapJobParams job_params {
        &m_config,
        m_scene,
        m_volume,
        m_sub_elements.ToSpan().Slice(start_index, end_index - start_index),
        &m_sub_elements_by_entity,
        acceleration_structure
    };

    job_params.renderers.Resize(m_lightmap_renderers.Size());

    for (SizeType i = 0; i < m_lightmap_renderers.Size(); i++)
    {
        job_params.renderers[i] = m_lightmap_renderers[i].Get();
    }

    return job_params;
}

void Lightmapper::PerformLightmapping()
{
    HYP_SCOPE;
    const uint32 ideal_triangles_per_job = m_config.ideal_triangles_per_job;

    AssertThrowMsg(m_num_jobs.Get(MemoryOrder::ACQUIRE) == 0, "Cannot initialize lightmap renderer -- jobs currently running!");

    // Build jobs
    HYP_LOG(Lightmap, Info, "Building graph for lightmapper");

    EntityManager& mgr = *m_scene->GetEntityManager();

    m_sub_elements.Clear();
    m_sub_elements_by_entity.Clear();

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (!mesh_component.mesh.IsValid())
        {
            HYP_LOG(Lightmap, Info, "Skip entity with invalid mesh on MeshComponent");

            continue;
        }

        if (!mesh_component.material.IsValid())
        {
            HYP_LOG(Lightmap, Info, "Skip entity with invalid material on MeshComponent");

            continue;
        }

        // Only process opaque and translucent materials
        if (mesh_component.material->GetBucket() != BUCKET_OPAQUE && mesh_component.material->GetBucket() != BUCKET_TRANSLUCENT)
        {
            HYP_LOG(Lightmap, Info, "Skip entity with bucket that is not opaque or translucent");

            continue;
        }

        if (m_config.trace_mode == LightmapTraceMode::GPU_PATH_TRACING)
        {
            if (!mesh_component.raytracing_data)
            {
                HYP_LOG(Lightmap, Info, "Skipping Entity {} because it has no raytracing data set", entity_id.Value());

                continue;
            }
        }

        Handle<Entity> entity { entity_id };
        AssertThrow(entity.IsValid());

        m_sub_elements.PushBack(LightmapSubElement {
            entity,
            mesh_component.mesh,
            mesh_component.material,
            transform_component.transform,
            bounding_box_component.world_aabb });
    }

    AssertThrow(m_acceleration_structure == nullptr);
    m_acceleration_structure = MakeUnique<LightmapTopLevelAccelerationStructure>();

    if (m_sub_elements.Empty())
    {
        return;
    }

    for (LightmapSubElement& sub_element : m_sub_elements)
    {
        BVHComponent* bvh_component = mgr.TryGetComponent<BVHComponent>(sub_element.entity);

        if (!bvh_component)
        {
            // BVHUpdaterSystem will calculate BVH when added
            mgr.AddComponent<BVHComponent>(sub_element.entity, BVHComponent {});

            bvh_component = &mgr.GetComponent<BVHComponent>(sub_element.entity);
        }

        m_acceleration_structure->Add(&sub_element, &bvh_component->bvh);
    }

    uint32 num_triangles = 0;
    SizeType start_index = 0;

    for (SizeType index = 0; index < m_sub_elements.Size(); index++)
    {
        LightmapSubElement& sub_element = m_sub_elements[index];

        m_sub_elements_by_entity.Set(sub_element.entity, &sub_element);

        if (ideal_triangles_per_job != 0 && num_triangles != 0 && num_triangles + sub_element.mesh->NumIndices() / 3 > ideal_triangles_per_job)
        {
            UniquePtr<LightmapJob> job = MakeUnique<LightmapJob>(CreateLightmapJobParams(start_index, index + 1, m_acceleration_structure.Get()));

            start_index = index + 1;

            AddJob(std::move(job));

            num_triangles = 0;
        }

        num_triangles += sub_element.mesh->NumIndices() / 3;
    }

    if (start_index < m_sub_elements.Size() - 1)
    {
        UniquePtr<LightmapJob> job = MakeUnique<LightmapJob>(CreateLightmapJobParams(start_index, m_sub_elements.Size(), m_acceleration_structure.Get()));

        AddJob(std::move(job));
    }
}

void Lightmapper::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    uint32 num_jobs = m_num_jobs.Get(MemoryOrder::ACQUIRE);

    Mutex::Guard guard(m_queue_mutex);

    AssertThrow(!m_queue.Empty());
    LightmapJob* job = m_queue.Front().Get();

    // Start job if not started
    if (!job->IsRunning())
    {
        job->Start();

        m_scene->GetWorld()->AddView(job->GetView());
    }

    job->Process();

    if (job->IsCompleted())
    {
        HandleCompletedJob(job);
    }
}

void Lightmapper::HandleCompletedJob(LightmapJob* job)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    m_scene->GetWorld()->RemoveView(job->GetView());

    if (job->GetResult().HasError())
    {
        HYP_LOG(Lightmap, Error, "Lightmap job {} failed with error: {}", job->GetUUID(), job->GetResult().GetError().GetMessage());

        m_queue.Pop();
        m_num_jobs.Decrement(1, MemoryOrder::RELEASE);

        return;
    }

    HYP_LOG(Lightmap, Debug, "Tracing completed for lightmapping job {} ({} subelements)", job->GetUUID(), job->GetSubElements().Size());

    const LightmapUVMap& uv_map = job->GetUVMap();
    const uint32 element_index = job->GetElementIndex();

    if (!m_volume->BuildElementTextures(uv_map, element_index))
    {
        HYP_LOG(Lightmap, Error, "Failed to build LightmapElement textures for LightmapVolume, element index: {}", job->GetElementIndex());

        return;
    }

    const LightmapElement* element = m_volume->GetElement(element_index);
    AssertThrow(element != nullptr);

    HYP_LOG(Lightmap, Debug, "Lightmap job {}: Building element with index {}, UV offset: {}, Scale: {}", job->GetUUID(), element_index,
        element->offset_uv, element->scale);

    for (SizeType sub_element_index = 0; sub_element_index < job->GetSubElements().Size(); sub_element_index++)
    {
        LightmapSubElement& sub_element = job->GetSubElements()[sub_element_index];

        auto update_mesh_data = [&]()
        {
            const Handle<Mesh>& mesh = sub_element.mesh;
            AssertThrow(mesh.IsValid());

            AssertThrow(sub_element_index < job->GetUVBuilder().GetMeshData().Size());

            const LightmapMeshData& lightmap_mesh_data = job->GetUVBuilder().GetMeshData()[sub_element_index];
            AssertThrow(lightmap_mesh_data.mesh == mesh);

            MeshData new_mesh_data;
            new_mesh_data.vertices = lightmap_mesh_data.vertices;
            new_mesh_data.indices = lightmap_mesh_data.indices;

            for (uint32 i = 0; i < new_mesh_data.vertices.Size(); i++)
            {
                Vec2f& lightmap_uv = new_mesh_data.vertices[i].texcoord1;
                lightmap_uv.y = 1.0f - lightmap_uv.y; // Invert Y coordinate for lightmaps
                lightmap_uv *= element->scale;
                lightmap_uv += Vec2f(element->offset_uv.x, element->offset_uv.y);
            }

            ResourceHandle resource_handle;
            mesh->SetStreamedMeshData(MakeRefCountedPtr<StreamedMeshData>(std::move(new_mesh_data), resource_handle));
        };

        update_mesh_data();

        bool is_new_material = false;

        sub_element.material = sub_element.material.IsValid() ? sub_element.material->Clone() : CreateObject<Material>();
        is_new_material = true;

        sub_element.material->SetBucket(BUCKET_LIGHTMAP);

#if 0
        // temp ; testing. - will instead be set in the lightmap volume
        Bitmap<4, float> radiance_bitmap = uv_map.ToBitmapRadiance();
        Bitmap<4, float> irradiance_bitmap = uv_map.ToBitmapIrradiance();

        Handle<Texture> irradiance_texture = CreateObject<Texture>(TextureData {
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA32F,
                Vec3u { uv_map.width, uv_map.height, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_REPEAT },
            ByteBuffer(irradiance_bitmap.GetUnpackedFloats().ToByteView()) });
        InitObject(irradiance_texture);

        Handle<Texture> radiance_texture = CreateObject<Texture>(TextureData {
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA32F,
                Vec3u { uv_map.width, uv_map.height, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_REPEAT },
            ByteBuffer(radiance_bitmap.GetUnpackedFloats().ToByteView()) });
        InitObject(radiance_texture);

        sub_element.material->SetTexture(MaterialTextureKey::IRRADIANCE_MAP, irradiance_texture);
        sub_element.material->SetTexture(MaterialTextureKey::RADIANCE_MAP, radiance_texture);
#else
        // @TEMP
        sub_element.material->SetTexture(MaterialTextureKey::IRRADIANCE_MAP, m_volume->GetAtlasTextures().Get(LightmapElementTextureType::IRRADIANCE).second);
        sub_element.material->SetTexture(MaterialTextureKey::RADIANCE_MAP, m_volume->GetAtlasTextures().Get(LightmapElementTextureType::RADIANCE).second);
#endif

        auto update_mesh_component = [entity_manager_weak = m_scene->GetEntityManager()->WeakHandleFromThis(), element_index = job->GetElementIndex(), volume = m_volume, sub_element = sub_element, new_material = (is_new_material ? sub_element.material : Handle<Material>::empty)]()
        {
            Handle<EntityManager> entity_manager = entity_manager_weak.Lock();

            if (!entity_manager)
            {
                HYP_LOG(Lightmap, Error, "Failed to lock EntityManager while updating lightmap element");

                return;
            }

            const Handle<Entity>& entity = sub_element.entity;

            if (entity_manager->HasComponent<MeshComponent>(entity))
            {
                MeshComponent& mesh_component = entity_manager->GetComponent<MeshComponent>(entity);

                if (new_material.IsValid())
                {
                    InitObject(new_material);

                    mesh_component.material = std::move(new_material);
                }

                mesh_component.lightmap_volume = volume.ToWeak();
                mesh_component.lightmap_element_index = element_index;
                mesh_component.lightmap_volume_uuid = volume->GetUUID();
            }
            else
            {
                AssertThrow(new_material.IsValid());
                InitObject(new_material);

                MeshComponent mesh_component {};
                mesh_component.mesh = sub_element.mesh;
                mesh_component.material = new_material;
                mesh_component.lightmap_volume = volume.ToWeak();
                mesh_component.lightmap_element_index = element_index;
                mesh_component.lightmap_volume_uuid = volume->GetUUID();

                entity_manager->AddComponent<MeshComponent>(entity, std::move(mesh_component));
            }

            entity_manager->AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
        };

        if (Threads::IsOnThread(m_scene->GetEntityManager()->GetOwnerThreadID()))
        {
            // If we are on the same thread, we can update the mesh component immediately
            update_mesh_component();
        }
        else
        {
            // Enqueue the update to be performed on the owner thread
            ThreadBase* thread = Threads::GetThread(m_scene->GetEntityManager()->GetOwnerThreadID());
            AssertThrow(thread != nullptr);

            thread->GetScheduler().Enqueue(std::move(update_mesh_component), TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }

    m_queue.Pop();
    m_num_jobs.Decrement(1, MemoryOrder::RELEASE);
}

#pragma endregion Lightmapper

} // namespace hyperion