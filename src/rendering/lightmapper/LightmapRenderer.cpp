#include <rendering/lightmapper/LightmapRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

#pragma region Render commands

struct RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer) : renderer::RenderCommand
{
    GPUBufferRef uniform_buffer;

    RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer)(GPUBufferRef uniform_buffer)
        : uniform_buffer(std::move(uniform_buffer))
    {
    }

    virtual ~RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer)() override = default;

    virtual Result operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms)));
        uniform_buffer->Memset(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms), 0x0);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

// LightmapPathTracer

LightmapPathTracer::LightmapPathTracer(Handle<TLAS> tlas, LightmapShadingType shading_type)
    : m_tlas(std::move(tlas)),
      m_shading_type(shading_type),
      m_uniform_buffers({
          MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::CONSTANT_BUFFER),
          MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::CONSTANT_BUFFER)
      }),
      m_rays_buffers({
          MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER),
          MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER)
      }),
      m_hits_buffers({
          MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER),
          MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER)
      }),
      m_raytracing_pipeline(MakeRenderObject<renderer::RaytracingPipeline>())
{
    
}

LightmapPathTracer::~LightmapPathTracer()
{
    SafeRelease(std::move(m_uniform_buffers));
    SafeRelease(std::move(m_rays_buffers));
    SafeRelease(std::move(m_hits_buffers));
    SafeRelease(std::move(m_raytracing_pipeline));
}

void LightmapPathTracer::CreateUniformBuffer()
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_uniform_buffers[frame_index] = MakeRenderObject<GPUBuffer>(UniformBuffer());

        PUSH_RENDER_COMMAND(CreateLightmapPathTracerUniformBuffer, m_uniform_buffers[frame_index]);
    }
}

void LightmapPathTracer::Create()
{
    CreateUniformBuffer();

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        DeferCreate(
            m_hits_buffers[frame_index],
            g_engine->GetGPUDevice(),
            sizeof(LightmapHitsBuffer)
        );

        DeferCreate(
            m_rays_buffers[frame_index],
            g_engine->GetGPUDevice(),
            sizeof(Vec4f) * 2
        );
    }

    ShaderProperties shader_properties;

    switch (m_shading_type) {
    case LIGHTMAP_SHADING_TYPE_RADIANCE:
        shader_properties.Set("MODE_RADIANCE");
        break;
    case LIGHTMAP_SHADING_TYPE_IRRADIANCE:
        shader_properties.Set("MODE_IRRADIANCE");
        break;
    }

    Handle<Shader> shader = g_shader_manager->GetOrCreate(HYP_NAME(LightmapPathTracer), shader_properties);

    if (!InitObject(shader)) {
        return;
    }

    renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSet2Ref &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(RTRadianceDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(HYP_NAME(TLAS), m_tlas->GetInternalTLAS());
        descriptor_set->SetElement(HYP_NAME(MeshDescriptionsBuffer), m_tlas->GetInternalTLAS()->GetMeshDescriptionsBuffer());
        descriptor_set->SetElement(HYP_NAME(HitsBuffer), m_hits_buffers[frame_index]);
        descriptor_set->SetElement(HYP_NAME(RaysBuffer), m_rays_buffers[frame_index]);

        descriptor_set->SetElement(HYP_NAME(LightsBuffer), g_engine->GetRenderData()->lights.GetBuffer());
        descriptor_set->SetElement(HYP_NAME(MaterialsBuffer), g_engine->GetRenderData()->materials.GetBuffer());

        descriptor_set->SetElement(HYP_NAME(RTRadianceUniforms), m_uniform_buffers[frame_index]);
    }

    DeferCreate(
        descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_raytracing_pipeline = MakeRenderObject<RaytracingPipeline>(
        shader->GetShaderProgram(),
        descriptor_table
    );

    DeferCreate(
        m_raytracing_pipeline,
        g_engine->GetGPUDevice()
    );
}

void LightmapPathTracer::UpdateUniforms(Frame *frame, uint32 ray_offset)
{
    RTRadianceUniforms uniforms { };
    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    uniforms.ray_offset = ray_offset;

    const uint32 num_bound_lights = MathUtil::Min(uint32(g_engine->GetRenderState().lights.Size()), 16);

    for (uint32 index = 0; index < num_bound_lights; index++) {
        uniforms.light_indices[index] = g_engine->GetRenderState().lights.AtIndex(index).first.ToIndex();
    }

    uniforms.num_bound_lights = num_bound_lights;

    m_uniform_buffers[frame->GetFrameIndex()]->Copy(g_engine->GetGPUDevice(), sizeof(uniforms), &uniforms);
}

void LightmapPathTracer::ReadHitsBuffer(LightmapHitsBuffer *ptr, uint frame_index)
{
    m_hits_buffers[frame_index]->Read(
        g_engine->GetGPUDevice(),
        sizeof(LightmapHitsBuffer),
        ptr
    );

    //Memory::MemCpy(ptr, &m_previous_hits_buffers[frame_index], sizeof(LightmapHitsBuffer));
}

void LightmapPathTracer::Trace(Frame *frame, const Array<LightmapRay> &rays, uint32 ray_offset)
{
    const uint frame_index = frame->GetFrameIndex();
    const uint previous_frame_index = (frame->GetFrameIndex() + max_frames_in_flight - 1) % max_frames_in_flight;

    /*m_hits_buffers[previous_frame_index]->Read(
        g_engine->GetGPUDevice(),
        sizeof(LightmapHitsBuffer),
        &m_previous_hits_buffers[previous_frame_index]
    );*/

    UpdateUniforms(frame, ray_offset);

    { // rays buffer
        Array<float> ray_float_data;
        ray_float_data.Resize(rays.Size() * 8);

        for (uint i = 0; i < rays.Size(); i++) {
            ray_float_data[i * 8 + 0] = rays[i].ray.position.x;
            ray_float_data[i * 8 + 1] = rays[i].ray.position.y;
            ray_float_data[i * 8 + 2] = rays[i].ray.position.z;
            ray_float_data[i * 8 + 3] = 1.0f;
            ray_float_data[i * 8 + 4] = rays[i].ray.direction.x;
            ray_float_data[i * 8 + 5] = rays[i].ray.direction.y;
            ray_float_data[i * 8 + 6] = rays[i].ray.direction.z;
            ray_float_data[i * 8 + 7] = 0.0f;
        }
        
        bool rays_buffer_resized = false;

        HYPERION_ASSERT_RESULT(m_rays_buffers[frame->GetFrameIndex()]->EnsureCapacity(g_engine->GetGPUDevice(), ray_float_data.ByteSize(), &rays_buffer_resized));
        m_rays_buffers[frame->GetFrameIndex()]->Copy(g_engine->GetGPUDevice(), ray_float_data.ByteSize(), ray_float_data.Data());

        if (rays_buffer_resized) {
            m_raytracing_pipeline->GetDescriptorTable().Get()->GetDescriptorSet(HYP_NAME(RTRadianceDescriptorSet), frame->GetFrameIndex())
                ->SetElement(HYP_NAME(RaysBuffer), m_rays_buffers[frame->GetFrameIndex()]);

            HYPERION_ASSERT_RESULT(m_raytracing_pipeline->GetDescriptorTable().Get()->Update(g_engine->GetGPUDevice(), frame->GetFrameIndex()));
        }
    }
    
    m_raytracing_pipeline->Bind(frame->GetCommandBuffer());

    m_raytracing_pipeline->GetDescriptorTable().Get()->Bind(
        frame,
        m_raytracing_pipeline,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_hits_buffers[frame->GetFrameIndex()]->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::UNORDERED_ACCESS
    );

    m_raytracing_pipeline->TraceRays(
        g_engine->GetGPUDevice(),
        frame->GetCommandBuffer(),
        Extent3D { uint32(rays.Size()), 1, 1 }
    );

    m_hits_buffers[frame->GetFrameIndex()]->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::UNORDERED_ACCESS
    );
}

// LightmapJob

LightmapJob::LightmapJob(Scene *scene, Array<LightmapEntity> entities)
    : m_scene(scene),
      m_entities(std::move(entities)),
      m_is_started { false },
      m_is_ready { false },
      m_texel_index(0)
{
}

LightmapJob::LightmapJob(Scene *scene, Array<LightmapEntity> entities, HashMap<ID<Mesh>, Array<Triangle>> triangle_cache)
    : LightmapJob(scene, std::move(entities))
{
    m_triangle_cache = std::move(triangle_cache);
}

void LightmapJob::Start()
{
    if (IsStarted()) {
        return;
    }

    m_is_started.Set(true, MemoryOrder::RELAXED);

    BuildUVMap();

    // Flatten texel indices, grouped by mesh IDs
    const LightmapUVMap &uv_map = GetUVMap();
    m_texel_indices.Reserve(uv_map.uvs.Size());

    for (const auto &it : uv_map.mesh_to_uv_indices) {
        for (uint i = 0; i < it.second.Size(); i++) {
             m_texel_indices.PushBack(it.second[i]);
        }
    }

    m_is_ready.Set(true, MemoryOrder::RELAXED);
}

bool LightmapJob::IsStarted() const
{
    return m_is_started.Get(MemoryOrder::RELAXED);
}

bool LightmapJob::IsCompleted() const
{
    if (!IsReady() || !IsStarted()) {
        return false;
    }

    if (m_entities.Empty()) {
        return true;
    }

    // Ensure there are no rays remaining to be integrated
    for (uint i = 0; i < max_frames_in_flight; i++) {
        if (m_previous_frame_rays[i].Any()) {
            return false;
        }
    }

    if (m_texel_index >= m_texel_indices.Size() * num_multisamples) {
        return true;
    }

    return false;
}

void LightmapJob::BuildUVMap()
{
    LightmapUVBuilder uv_builder { { m_entities } };

    auto uv_builder_result = uv_builder.Build();

    // @TODO Handle bad result

    m_uv_map = std::move(uv_builder_result.uv_map);
}

void LightmapJob::GatherRays(uint max_ray_hits, Array<LightmapRay> &out_rays)
{
    if (!IsReady()) {
        return;
    }

    if (IsCompleted()) {
        return;
    }

    Optional<Pair<ID<Mesh>, StreamedDataRef<StreamedMeshData>>> streamed_mesh_data { };

    uint ray_index = 0;

    while (ray_index < max_ray_hits) {
        if (m_texel_index >= m_texel_indices.Size() * num_multisamples) {
            break;
        }

        const LightmapUV &uv = m_uv_map.uvs[m_texel_indices[m_texel_index % m_texel_indices.Size()]];

        Handle<Mesh> mesh = Handle<Mesh>(uv.mesh_id);

        if (!mesh.IsValid()) {
            ++m_texel_index;
            continue;
        }

        if (!mesh->GetStreamedMeshData()) {
            ++m_texel_index;
            continue;
        }

        if (!streamed_mesh_data.HasValue() || streamed_mesh_data->first != mesh.GetID()) {
            streamed_mesh_data.Set({
                mesh.GetID(),
                mesh->GetStreamedMeshData()->AcquireRef()
            });
        }

        // Convert UV to world space
        const MeshData &mesh_data = streamed_mesh_data->second->GetMeshData();

        AssertThrowMsg(
            uv.triangle_index * 3 + 2 < mesh_data.indices.Size(),
            "Triangle index (%u) out of range of mesh indices",
            uv.triangle_index
        );

        const Matrix4 normal_matrix = uv.transform.Inverted().Transpose();

        const Vec3f vertex_positions[3] = {
            uv.transform * mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 0]].position,
            uv.transform * mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 1]].position,
            uv.transform * mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 2]].position
        };

        const Vec3f vertex_normals[3] = {
            (normal_matrix * Vec4f(mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 0]].normal, 0.0f)).GetXYZ(),
            (normal_matrix * Vec4f(mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 1]].normal, 0.0f)).GetXYZ(),
            (normal_matrix * Vec4f(mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 2]].normal, 0.0f)).GetXYZ()
        };

        const Vec3f position = vertex_positions[0] * uv.barycentric_coords.x
            + vertex_positions[1] * uv.barycentric_coords.y
            + vertex_positions[2] * uv.barycentric_coords.z;
        
        const Vec3f normal = (vertex_normals[0] * uv.barycentric_coords.x
            + vertex_normals[1] * uv.barycentric_coords.y
            + vertex_normals[2] * uv.barycentric_coords.z).Normalize();

        out_rays.PushBack(LightmapRay {
            Ray {
                position + normal * 0.01f,
                normal
            },
            mesh.GetID(),
            uv.triangle_index,
            m_texel_indices[m_texel_index % m_texel_indices.Size()]
        });
        
        ++m_texel_index;
        ++ray_index;
    }
}

void LightmapJob::IntegrateRayHits(const LightmapRay *rays, const LightmapHit *hits, uint num_hits, LightmapShadingType shading_type)
{
    for (uint i = 0; i < num_hits; i++) {
        const LightmapRay &ray = rays[i];
        const LightmapHit &hit = hits[i];

        LightmapUVMap &uv_map = GetUVMap();
        LightmapUV &uv = uv_map.uvs[ray.texel_index];

        switch (shading_type) {
        case LIGHTMAP_SHADING_TYPE_RADIANCE:
            uv.radiance = (uv.radiance * (Vec4f(1.0f) - Vec4f(hit.color.w))) + Vec4f(hit.color * hit.color.w);
            break;
        case LIGHTMAP_SHADING_TYPE_IRRADIANCE:
            uv.irradiance = (uv.irradiance * (Vec4f(1.0f) - Vec4f(hit.color.w))) + Vec4f(hit.color * hit.color.w);
            break;
        }

        //uv.color = (uv.color * (Vec4f(1.0f) - Vec4f(hit.color.w))) + Vec4f(hit.color * hit.color.w);
    }
}

Optional<LightmapHit> LightmapJob::TraceSingleRayOnCPU(const LightmapRay &ray)
{
    RayTestResults octree_results;

    if (m_scene->GetOctree().TestRay(ray.ray, octree_results)) {
        // distance, hit
        FlatMap<float, LightmapHit> results;

        for (const RayHit &hit : octree_results) {
            // now ray test each result as triangle mesh to find exact hit point
            if (ID<Entity> entity_id = ID<Entity>(hit.id)) {
                const MeshComponent *mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(entity_id);
                const TransformComponent *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(entity_id);

                if (mesh_component != nullptr && mesh_component->mesh.IsValid() && mesh_component->material.IsValid() && mesh_component->mesh->NumIndices() != 0 && transform_component != nullptr) {
                    const ID<Mesh> mesh_id = mesh_component->mesh.GetID();

                    auto triangle_cache_it = m_triangle_cache.Find(mesh_id);
                    if (triangle_cache_it == m_triangle_cache.End()) {
                        continue;
                    }

                    const Optional<RayHit> triangle_hit = ray.ray.TestTriangleList(
                        triangle_cache_it->second,
                        transform_component->transform
                    );

                    if (triangle_hit.HasValue()) {
                        // Sample albedo and return as LightmapHit
                        const uint triangle_index = triangle_hit->id;
                        const Vec3f barycentric_coords = triangle_hit->barycentric_coords;

                        const Triangle &triangle = triangle_cache_it->second[triangle_index];

                        const Vec2f uv = triangle.GetPoint(0).texcoord0 * barycentric_coords.x
                            + triangle.GetPoint(1).texcoord0 * barycentric_coords.y
                            + triangle.GetPoint(2).texcoord0 * barycentric_coords.z;

                        const Vec4f color = Vec4f(mesh_component->material->GetParameter(Material::MATERIAL_KEY_ALBEDO));

                        // @TODO Sample albedo from texture

                        auto insert_result = results.Insert({
                            triangle_hit->distance,
                            {
                                color
                            }
                        });

                        // @TODO Recursion
                    }
                }
            }
        }

        if (!results.Empty()) {
            return results.Front().second;
        }
    }

    return { };
}

void LightmapJob::TraceRaysOnCPU(const Array<LightmapRay> &rays, LightmapShadingType shading_type)
{
    rays.ParallelForEach(TaskSystem::GetInstance(), [this, shading_type](const LightmapRay &ray, uint index, uint batch_index)
    {
        Optional<LightmapHit> hit = TraceSingleRayOnCPU(ray);

        if (hit.HasValue()) {
            IntegrateRayHits(&ray, &hit.Get(), 1, shading_type);
        }
    });
}

// LightmapRenderer

LightmapRenderer::LightmapRenderer(Name name)
    : RenderComponent(name),
      m_trace_mode(LIGHTMAP_TRACE_MODE_CPU),
      m_num_jobs { 0u }
{
}

void LightmapRenderer::Init()
{
    if (g_engine->GetConfig().Get(CONFIG_RT_ENABLED)) {
        // trace on GPU if the card supports ray tracing
        m_trace_mode = LIGHTMAP_TRACE_MODE_GPU;
    }
}

void LightmapRenderer::InitGame()
{
    static constexpr uint ideal_triangles_per_job = 10000;

    // Build jobs
    m_parent->GetScene()->GetEntityManager()->PushCommand([this](EntityManager &mgr, GameCounter::TickUnit)
    {
        Array<LightmapEntity> lightmap_entities;
        HashMap<ID<Mesh>, Array<Triangle>> triangle_cache;
        uint num_triangles = 0;

        for (auto [entity, mesh_component, transform_component] : mgr.GetEntitySet<MeshComponent, TransformComponent>()) {
            if (!mesh_component.mesh.IsValid()) {
                continue;
            }

            if (!mesh_component.material.IsValid()) {
                continue;
            }

            // Only process opaque and translucent materials
            if (mesh_component.material->GetBucket() != BUCKET_OPAQUE && mesh_component.material->GetBucket() != BUCKET_TRANSLUCENT) {
                continue;
            }

            const RC<StreamedMeshData> &streamed_mesh_data = mesh_component.mesh->GetStreamedMeshData();

            if (!streamed_mesh_data) {
                continue;
            }

            if (ideal_triangles_per_job != 0 && num_triangles + streamed_mesh_data->GetMeshData().indices.Size() / 3 > ideal_triangles_per_job) {
                if (lightmap_entities.Any()) {
                    UniquePtr<LightmapJob> job(new LightmapJob(m_parent->GetScene(), std::move(lightmap_entities), std::move(triangle_cache)));

                    AddJob(std::move(job));
                }

                num_triangles = 0;
            }

            // Set triangle data in m_triangle_cache
            // On CPU, we need to cache the triangles for ray tracing
            if (m_trace_mode == LIGHTMAP_TRACE_MODE_CPU) {
                const Matrix4 &transform_matrix = transform_component.transform.GetMatrix();

                auto ref = streamed_mesh_data->AcquireRef();
                const MeshData &mesh_data = ref->GetMeshData();

                const Matrix4 normal_matrix = transform_matrix.Inverted().Transpose();

                for (uint i = 0; i < mesh_data.indices.Size(); i += 3) {
                    Triangle triangle {
                        mesh_data.vertices[mesh_data.indices[i + 0]],
                        mesh_data.vertices[mesh_data.indices[i + 1]],
                        mesh_data.vertices[mesh_data.indices[i + 2]]
                    };

                    triangle[0].position = transform_matrix * triangle[0].position;
                    triangle[1].position = transform_matrix * triangle[1].position;
                    triangle[2].position = transform_matrix * triangle[2].position;

                    triangle[0].normal = (normal_matrix * Vec4f(triangle[0].normal, 0.0f)).GetXYZ();
                    triangle[1].normal = (normal_matrix * Vec4f(triangle[1].normal, 0.0f)).GetXYZ();
                    triangle[2].normal = (normal_matrix * Vec4f(triangle[2].normal, 0.0f)).GetXYZ();

                    triangle[0].tangent = (normal_matrix * Vec4f(triangle[0].tangent, 0.0f)).GetXYZ();
                    triangle[1].tangent = (normal_matrix * Vec4f(triangle[1].tangent, 0.0f)).GetXYZ();
                    triangle[2].tangent = (normal_matrix * Vec4f(triangle[2].tangent, 0.0f)).GetXYZ();

                    triangle[0].bitangent = (normal_matrix * Vec4f(triangle[0].bitangent, 0.0f)).GetXYZ();
                    triangle[1].bitangent = (normal_matrix * Vec4f(triangle[1].bitangent, 0.0f)).GetXYZ();
                    triangle[2].bitangent = (normal_matrix * Vec4f(triangle[2].bitangent, 0.0f)).GetXYZ();

                    triangle_cache[mesh_component.mesh.GetID()].PushBack(triangle);
                }
            }

            lightmap_entities.PushBack(LightmapEntity {
                entity,
                mesh_component.mesh,
                mesh_component.material,
                transform_component.transform.GetMatrix()
            });

            num_triangles += streamed_mesh_data->GetMeshData().indices.Size() / 3;
        }

        if (lightmap_entities.Any()) {
            UniquePtr<LightmapJob> job(new LightmapJob(m_parent->GetScene(), std::move(lightmap_entities), std::move(triangle_cache)));

            AddJob(std::move(job));
        }
    }); 
}

void LightmapRenderer::OnRemoved()
{
    m_path_tracer_radiance.Reset();
    m_path_tracer_irradiance.Reset();

    Mutex::Guard guard(m_queue_mutex);

    m_queue.Clear();

    m_num_jobs.Set(0u, MemoryOrder::RELAXED);
}

void LightmapRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    if (!m_num_jobs.Get(MemoryOrder::RELAXED)) {
        return;
    }

    DebugLog(
        LogType::Debug,
        "Processing %u lightmap jobs...\n",
        m_num_jobs.Get(MemoryOrder::RELAXED)
    );

    // Trace lightmap on CPU

    Mutex::Guard guard(m_queue_mutex);

    AssertThrow(!m_queue.Empty());
    LightmapJob *job = m_queue.Front().Get();

    if (job->IsCompleted()) {
        HandleCompletedJob(job);

        return;
    }

    // Start job if not started
    if (!job->IsStarted()) {
        job->Start();
    }

    // If in CPU mode, trace rays on CPU
    if (m_trace_mode == LIGHTMAP_TRACE_MODE_CPU) {
        Array<LightmapRay> rays;

        job->GatherRays(max_ray_hits_cpu, rays);

        if (rays.Any()) {
            job->TraceRaysOnCPU(rays, LIGHTMAP_SHADING_TYPE_IRRADIANCE);
            job->TraceRaysOnCPU(rays, LIGHTMAP_SHADING_TYPE_RADIANCE);
        }
    }
}

void LightmapRenderer::OnRender(Frame *frame)
{
    const uint frame_index = frame->GetFrameIndex();
    const uint previous_frame_index = (frame_index + max_frames_in_flight - 1) % max_frames_in_flight;

    // Do nothing if not in GPU trace mode
    if (m_trace_mode != LIGHTMAP_TRACE_MODE_GPU) {
        return;
    }

    if (!m_num_jobs.Get(MemoryOrder::RELAXED)) {
        return;
    }

    if (!m_path_tracer_radiance) {
        m_path_tracer_radiance.Reset(new LightmapPathTracer(m_parent->GetScene()->GetTLAS(), LIGHTMAP_SHADING_TYPE_RADIANCE));
        m_path_tracer_radiance->Create();
    }

    if (!m_path_tracer_irradiance) {
        m_path_tracer_irradiance.Reset(new LightmapPathTracer(m_parent->GetScene()->GetTLAS(), LIGHTMAP_SHADING_TYPE_IRRADIANCE));
        m_path_tracer_irradiance->Create();
    }

    // Wait for path tracer to be ready to process rays
    if (!m_path_tracer_radiance->GetPipeline()->IsCreated() || !m_path_tracer_irradiance->GetPipeline()->IsCreated()) {
        return;
    }
    
    Array<LightmapRay> current_frame_rays;
    uint32 ray_offset = 0;

    {
        Mutex::Guard guard(m_queue_mutex);

        // Hack: ensure num_jobs has not changed
        if (!m_num_jobs.Get(MemoryOrder::RELAXED)) {
            return;
        }

        LightmapJob *job = m_queue.Front().Get();

        if (job->IsCompleted()) {
            return;
        }

        // Wait for job to be ready
        if (!job->IsReady()) {
            return;
        }

        // Read ray hits from last time this frame was rendered
        const Array<LightmapRay> &previous_rays = job->GetPreviousFrameRays(frame_index);
        
        // Read previous frame hits into CPU buffer
        if (previous_rays.Any()) {
            // @NOTE Use heap allocation to avoid stack overflow (max_ray_hits_gpu * sizeof(LightmapHit) > 1MB)
            LightmapHitsBuffer *hits_buffer(new LightmapHitsBuffer);

            m_path_tracer_radiance->ReadHitsBuffer(hits_buffer, frame_index);
            job->IntegrateRayHits(previous_rays.Data(), hits_buffer->hits.Data(), previous_rays.Size(), LIGHTMAP_SHADING_TYPE_RADIANCE);

            m_path_tracer_irradiance->ReadHitsBuffer(hits_buffer, frame_index);
            job->IntegrateRayHits(previous_rays.Data(), hits_buffer->hits.Data(), previous_rays.Size(), LIGHTMAP_SHADING_TYPE_IRRADIANCE);

            delete hits_buffer;
        }

        ray_offset = job->GetTexelIndex() % MathUtil::Max(job->GetTexelIndices().Size(), 1u);

        job->GatherRays(max_ray_hits_gpu, current_frame_rays);

        job->SetPreviousFrameRays(frame_index, current_frame_rays);
    }

    if (current_frame_rays.Any()) {
        m_path_tracer_radiance->Trace(frame, current_frame_rays, ray_offset);
        m_path_tracer_irradiance->Trace(frame, current_frame_rays, ray_offset);
    }
}

void LightmapRenderer::HandleCompletedJob(LightmapJob *job)
{
    DebugLog(LogType::Debug, "Lightmap tracing completed. Writing bitmap...\n");

    const LightmapUVMap &uv_map = job->GetUVMap();

    FixedArray<Bitmap<4, float>, 2> bitmaps = {
        uv_map.ToBitmapRadiance(),
        uv_map.ToBitmapIrradiance()
    };

    for (auto &bitmap : bitmaps) {
        Bitmap<4, float> bitmap_dilated = bitmap;

        // Dilate lightmap
        for (uint x = 0; x < bitmap.GetWidth(); x++) {
            for (uint y = 0; y < bitmap.GetHeight(); y++) {
                Vec3f color = bitmap.GetPixel(x, y);
                color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x - 1, y - 1));
                color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x, y - 1));
                color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x + 1, y - 1));
                color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x - 1, y));
                color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x + 1, y));
                color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x - 1, y + 1));
                color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x, y + 1));
                color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x + 1, y + 1));

                bitmap_dilated.GetPixel(x, y) = color;
            }
        }

        bitmap = bitmap_dilated;

        // Apply bilateral blur
        Bitmap<4, float> bitmap_blurred = bitmap;

        for (uint x = 0; x < bitmap.GetWidth(); x++) {
            for (uint y = 0; y < bitmap.GetHeight(); y++) {
                Vec3f color = Vec3f(0.0f);

                float total_weight = 0.0f;

                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        const uint nx = x + dx;
                        const uint ny = y + dy;

                        if (nx >= bitmap.GetWidth() || ny >= bitmap.GetHeight()) {
                            continue;
                        }

                        const Vec3f neighbor_color = bitmap.GetPixel(nx, ny);

                        const float spatial_weight = MathUtil::Exp(-float(dx * dx + dy * dy) / (2.0f * 1.0f));
                        const float color_weight = MathUtil::Exp(-(neighbor_color - bitmap.GetPixel(x, y)).LengthSquared() / (2.0f * 0.1f));

                        const float weight = spatial_weight * color_weight;

                        color += neighbor_color * weight;
                        total_weight += weight;
                    }
                }

                bitmap_blurred.GetPixel(x, y) = color / MathUtil::Max(total_weight, MathUtil::epsilon_f);
            }
        }

        bitmap = bitmap_blurred;
    }
    
    // Temp; write to rgb8 bitmap
    uint num = rand() % 150;
    bitmaps[0].Write("lightmap_" + String::ToString(num) + "_radiance.bmp");
    bitmaps[1].Write("lightmap_" + String::ToString(num) + "_irradiance.bmp");

    FixedArray<Handle<Texture>, 2> textures;

    for (uint i = 0; i < 2; i++) {
        UniquePtr<StreamedData> streamed_data(new MemoryStreamedData(bitmaps[i].ToByteBuffer()));
        (void)streamed_data->Load();

        Handle<Texture> texture = CreateObject<Texture>(
            Extent3D { uv_map.width, uv_map.height, 1 },
            InternalFormat::RGBA32F,
            ImageType::TEXTURE_TYPE_2D,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_REPEAT,
            std::move(streamed_data)
        );

        InitObject(texture);

        textures[i] = std::move(texture);
    }

    for (const auto &it : job->GetEntities()) {
        if (!it.material) {
            // @TODO: Set to default material
            continue;
        }
        
        it.material->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_RADIANCE_MAP, textures[0]);
        it.material->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_IRRADIANCE_MAP, textures[1]);
    }

    m_queue.Pop();
    m_num_jobs.Decrement(1u, MemoryOrder::RELAXED);
}

} // namespace hyperion::v2