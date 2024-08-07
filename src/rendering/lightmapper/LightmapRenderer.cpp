/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/LightmapRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/system/AppContext.hpp>
#include <core/system/Time.hpp>

#include <Engine.hpp>

namespace hyperion {

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

#pragma endregion Render commands

#pragma region LightmapAccelerationStructure

struct LightmapRayHitData
{
    ID<Entity>  entity;
    Triangle    triangle;
    RayHit      hit;
};

using LightmapRayTestResults = FlatMap<float, LightmapRayHitData>;

class ILightmapAccelerationStructure
{
public:
    virtual ~ILightmapAccelerationStructure() = default;

    virtual LightmapRayTestResults TestRay(const Ray &ray) const = 0;
};

/// reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/bvh.html

class LightmapBVHNode
{
    static constexpr int max_depth = 3;

public:
    LightmapBVHNode(const BoundingBox &aabb)
        : m_aabb(aabb),
          m_is_leaf_node(true)
    {
    }

    LightmapBVHNode(const LightmapBVHNode &other)                   = delete;
    LightmapBVHNode &operator=(const LightmapBVHNode &other)        = delete;

    LightmapBVHNode(LightmapBVHNode &&other) noexcept               = default;
    LightmapBVHNode &operator=(LightmapBVHNode &&other) noexcept    = default;

    ~LightmapBVHNode()                                              = default;

    HYP_FORCE_INLINE const BoundingBox &GetAABB() const
        { return m_aabb; }

    HYP_FORCE_INLINE const Array<UniquePtr<LightmapBVHNode>> &GetChildren() const
        { return m_children; }

    HYP_FORCE_INLINE const Array<Triangle> &GetTriangles() const
        { return m_triangles; }

    HYP_FORCE_INLINE void AddTriangle(const Triangle &triangle)
        { m_triangles.PushBack(triangle); }

    HYP_FORCE_INLINE bool IsLeafNode() const
        { return m_is_leaf_node; }

    void Split()
    {
        Split(0);
    }

    HYP_NODISCARD RayTestResults TestRay(const Ray &ray) const
    {
        RayTestResults results;

        if (ray.TestAABB(m_aabb)) {
            if (IsLeafNode()) {
                for (SizeType triangle_index = 0; triangle_index < m_triangles.Size(); triangle_index++) {
                    const Triangle &triangle = m_triangles[triangle_index];

                    ray.TestTriangle(triangle, triangle_index, this, results);
                }
            } else {
                for (const UniquePtr<LightmapBVHNode> &node : m_children) {
                    results.Merge(node->TestRay(ray));
                }
            }
        }

        return results;
    }

    static void DebugLogBVHNode(LightmapBVHNode *node, int depth = 0)
    {
        String indentation_string;

        for (int i = 0; i < depth; i++) {
            indentation_string += "  ";
        }

        HYP_LOG(Lightmap, LogLevel::DEBUG, "{}Node {} (AABB: {}, {} triangles)", indentation_string, node->IsLeafNode() ? "(leaf)" : "(parent)", node->GetAABB(), node->m_triangles.Size());

        for (const UniquePtr<LightmapBVHNode> &node : node->m_children) {
            DebugLogBVHNode(node.Get(), depth + 1);
        }
    }

private:
    void Split(int depth)
    {
        if (m_is_leaf_node) {
            if (m_triangles.Any()) {
                if (depth < max_depth) {
                    const Vec3f center = m_aabb.GetCenter();
                    const Vec3f extent = m_aabb.GetExtent();

                    const Vec3f &min = m_aabb.GetMin();
                    const Vec3f &max = m_aabb.GetMax();

                    for (int i = 0; i < 2; i++) {
                        for (int j = 0; j < 2; j++) {
                            for (int k = 0; k < 2; k++) {
                                const Vec3f new_min = Vec3f(
                                    i == 0 ? min.x : center.x,
                                    j == 0 ? min.y : center.y,
                                    k == 0 ? min.z : center.z
                                );

                                const Vec3f new_max = Vec3f(
                                    i == 0 ? center.x : max.x,
                                    j == 0 ? center.y : max.y,
                                    k == 0 ? center.z : max.z
                                );

                                m_children.EmplaceBack(new LightmapBVHNode(BoundingBox(new_min, new_max)));
                            }
                        }
                    }

                    for (const Triangle &triangle : m_triangles) {
                        for (const UniquePtr<LightmapBVHNode> &node : m_children) {
                            if (node->GetAABB().ContainsTriangle(triangle)) {
                                node->m_triangles.PushBack(triangle);
                            }
                        }
                    }

                    m_triangles.Clear();

                    m_is_leaf_node = false;
                }
            }
        }

        for (const UniquePtr<LightmapBVHNode> &node : m_children) {
            node->Split(depth + 1);
        }
    }

    BoundingBox                         m_aabb;
    Array<UniquePtr<LightmapBVHNode>>   m_children;
    Array<Triangle>                     m_triangles;
    bool                                m_is_leaf_node;
};

class LightmapBottomLevelAccelerationStructure final : public ILightmapAccelerationStructure
{
public:
    LightmapBottomLevelAccelerationStructure(ID<Entity> entity, const Handle<Mesh> &mesh, const Transform &transform)
        : m_entity(entity),
          m_mesh(mesh),
          m_root(BuildBVH(mesh, transform))
    {
    }

    virtual ~LightmapBottomLevelAccelerationStructure() override = default;

    virtual LightmapRayTestResults TestRay(const Ray &ray) const override
    {
        LightmapRayTestResults results;

        if (m_root != nullptr) {
            const RayTestResults triangle_ray_test_results = m_root->TestRay(ray);

            for (const RayHit &ray_hit : triangle_ray_test_results) {
                AssertThrow(ray_hit.user_data != nullptr);

                const LightmapBVHNode *bvh_node = static_cast<const LightmapBVHNode *>(ray_hit.user_data);

                results.Insert(
                    ray_hit.distance,
                    LightmapRayHitData {
                        m_entity,
                        bvh_node->GetTriangles()[ray_hit.id],
                        ray_hit
                    }
                );
            }
        }

        return results;
    }

    HYP_FORCE_INLINE LightmapBVHNode *GetRoot() const
        { return m_root.Get(); }

private:
    static UniquePtr<LightmapBVHNode> BuildBVH(const Handle<Mesh> &mesh, const Transform &transform)
    {
        if (!mesh.IsValid()) {
            return nullptr;
        }

        if (!mesh->GetStreamedMeshData()) {
            return nullptr;
        }

        UniquePtr<LightmapBVHNode> root(new LightmapBVHNode(mesh->GetAABB() * transform));

        auto ref = mesh->GetStreamedMeshData()->AcquireRef();
        const MeshData &mesh_data = ref->GetMeshData();

        const Matrix4 &model_matrix = transform.GetMatrix();
        const Matrix4 normal_matrix = model_matrix.Inverted().Transpose();

        for (uint i = 0; i < mesh_data.indices.Size(); i += 3) {
            Triangle triangle {
                mesh_data.vertices[mesh_data.indices[i + 0]],
                mesh_data.vertices[mesh_data.indices[i + 1]],
                mesh_data.vertices[mesh_data.indices[i + 2]]
            };

            triangle[0].position = model_matrix * triangle[0].position;
            triangle[1].position = model_matrix * triangle[1].position;
            triangle[2].position = model_matrix * triangle[2].position;

            triangle[0].normal = (model_matrix * Vec4f(triangle[0].normal.Normalized(), 0.0f)).GetXYZ().Normalize();
            triangle[1].normal = (model_matrix * Vec4f(triangle[1].normal.Normalized(), 0.0f)).GetXYZ().Normalize();
            triangle[2].normal = (model_matrix * Vec4f(triangle[2].normal.Normalized(), 0.0f)).GetXYZ().Normalize();

            triangle[0].tangent = (model_matrix * Vec4f(triangle[0].tangent.Normalized(), 0.0f)).GetXYZ().Normalize();
            triangle[1].tangent = (model_matrix * Vec4f(triangle[1].tangent.Normalized(), 0.0f)).GetXYZ().Normalize();
            triangle[2].tangent = (model_matrix * Vec4f(triangle[2].tangent.Normalized(), 0.0f)).GetXYZ().Normalize();

            triangle[0].bitangent = (model_matrix * Vec4f(triangle[0].bitangent.Normalized(), 0.0f)).GetXYZ().Normalize();
            triangle[1].bitangent = (model_matrix * Vec4f(triangle[1].bitangent.Normalized(), 0.0f)).GetXYZ().Normalize();
            triangle[2].bitangent = (model_matrix * Vec4f(triangle[2].bitangent.Normalized(), 0.0f)).GetXYZ().Normalize();

            root->AddTriangle(triangle);
        }

        root->Split();

        // LightmapBVHNode::DebugLogBVHNode(root.Get());

        return root;
    }

    ID<Entity>                  m_entity;
    Handle<Mesh>                m_mesh;
    UniquePtr<LightmapBVHNode>  m_root;
};

class LightmapTopLevelAccelerationStructure final : public ILightmapAccelerationStructure
{
public:
    virtual ~LightmapTopLevelAccelerationStructure() override = default;

    virtual LightmapRayTestResults TestRay(const Ray &ray) const override
    {
        LightmapRayTestResults results;

        for (const UniquePtr<LightmapBottomLevelAccelerationStructure> &acceleration_structure : m_acceleration_structures) {
            results.Merge(acceleration_structure->TestRay(ray));
        }

        return results;
    }

    void Add(UniquePtr<LightmapBottomLevelAccelerationStructure> &&acceleration_structure)
    {
        if (!acceleration_structure) {
            return;
        }

        m_acceleration_structures.PushBack(std::move(acceleration_structure));
    }

private:
    Array<UniquePtr<LightmapBottomLevelAccelerationStructure>>  m_acceleration_structures;
};

#pragma endregion LightmapAccelerationStructure

#pragma region LightmapPathTracer

LightmapPathTracer::LightmapPathTracer(const TLASRef &tlas, LightmapShadingType shading_type)
    : m_tlas(tlas),
      m_shading_type(shading_type),
      m_uniform_buffers({
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::CONSTANT_BUFFER),
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::CONSTANT_BUFFER)
      }),
      m_rays_buffers({
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER),
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER)
      }),
      m_hits_buffers({
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER),
          MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STORAGE_BUFFER)
      }),
      m_raytracing_pipeline(MakeRenderObject<RaytracingPipeline>())
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

    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("LightmapPathTracer"), shader_properties);
    AssertThrow(shader.IsValid());

    renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("TLAS"), m_tlas);
        descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), m_tlas->GetMeshDescriptionsBuffer());
        descriptor_set->SetElement(NAME("HitsBuffer"), m_hits_buffers[frame_index]);
        descriptor_set->SetElement(NAME("RaysBuffer"), m_rays_buffers[frame_index]);

        descriptor_set->SetElement(NAME("LightsBuffer"), g_engine->GetRenderData()->lights.GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("MaterialsBuffer"), g_engine->GetRenderData()->materials.GetBuffer(frame_index));

        descriptor_set->SetElement(NAME("RTRadianceUniforms"), m_uniform_buffers[frame_index]);
    }

    DeferCreate(
        descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_raytracing_pipeline = MakeRenderObject<RaytracingPipeline>(
        shader,
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
            m_raytracing_pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex())
                ->SetElement(NAME("RaysBuffer"), m_rays_buffers[frame->GetFrameIndex()]);

            HYPERION_ASSERT_RESULT(m_raytracing_pipeline->GetDescriptorTable()->Update(g_engine->GetGPUDevice(), frame->GetFrameIndex()));
        }
    }
    
    m_raytracing_pipeline->Bind(frame->GetCommandBuffer());

    m_raytracing_pipeline->GetDescriptorTable()->Bind(
        frame,
        m_raytracing_pipeline,
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
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

#pragma endregion LightmapPathTracer

#pragma region LightmapJob

LightmapJob::LightmapJob(LightmapTraceMode trace_mode, Scene *scene, Span<LightmapEntity> entities_view, HashMap<ID<Entity>, LightmapEntity *> *all_entities_map)
    : m_trace_mode(trace_mode),
      m_scene(scene),
      m_entities_view(entities_view),
      m_all_entities_map(all_entities_map),
      m_is_started { false },
      m_is_ready { false },
      m_texel_index(0)
{
}

LightmapJob::LightmapJob(LightmapTraceMode trace_mode, Scene *scene,  Span<LightmapEntity> entities_view, HashMap<ID<Entity>, LightmapEntity *> *all_entities_map, UniquePtr<LightmapTopLevelAccelerationStructure> &&acceleration_structure)
    : LightmapJob(trace_mode, scene, entities_view, all_entities_map)
{
    m_acceleration_structure = std::move(acceleration_structure);
}

LightmapJob::~LightmapJob()
{
    for (Task<void> &task : m_current_tasks) {
        if (!task.Cancel()) {
            task.Await();
        }
    }
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

    if (!m_current_tasks.Every([](const Task<void> &task) { return task.IsCompleted(); })) {
        return false;
    }

    if (!m_entities_view) {
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
    LightmapUVBuilder uv_builder { { m_entities_view } };

    auto uv_builder_result = uv_builder.Build();

    // @TODO Handle bad result

    m_uv_map = std::move(uv_builder_result.uv_map);
}

void LightmapJob::Update()
{
    // If in CPU mode, trace rays on CPU
    if (m_trace_mode != LightmapTraceMode::LIGHTMAP_TRACE_MODE_CPU) {
        return;
    }

    if (m_current_tasks.Any()) {
        for (Task<void> &task : m_current_tasks) {
            if (!task.IsCompleted()) {
                // Wait for next call
                return;
            }
        }

        for (Task<void> &task : m_current_tasks) {
            task.Await();
        }

        m_current_tasks.Clear();
    }

    Array<LightmapRay> rays;

    GatherRays(max_ray_hits_cpu, rays);

    if (rays.Any()) {
        TraceRaysOnCPU(rays, LIGHTMAP_SHADING_TYPE_IRRADIANCE);
        TraceRaysOnCPU(rays, LIGHTMAP_SHADING_TYPE_RADIANCE);
    }
}

void LightmapJob::GatherRays(uint max_ray_hits, Array<LightmapRay> &out_rays)
{
    if (!IsReady()) {
        return;
    }

    if (IsCompleted()) {
        return;
    }

    Optional<Pair<ID<Mesh>, StreamedDataRef<StreamedMeshData>>> streamed_mesh_data_refs { };

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

        if (!streamed_mesh_data_refs.HasValue() || streamed_mesh_data_refs->first != mesh.GetID()) {
            streamed_mesh_data_refs.Set({
                mesh.GetID(),
                mesh->GetStreamedMeshData()->AcquireRef()
            });
        }

        // Convert UV to world space
        const MeshData &mesh_data = streamed_mesh_data_refs->second->GetMeshData();

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
            (Vec4f(mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 0]].normal, 0.0f)).GetXYZ(),
            (Vec4f(mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 1]].normal, 0.0f)).GetXYZ(),
            (Vec4f(mesh_data.vertices[mesh_data.indices[uv.triangle_index * 3 + 2]].normal, 0.0f)).GetXYZ()
        };

        const Vec3f position = vertex_positions[0] * uv.barycentric_coords.x
            + vertex_positions[1] * uv.barycentric_coords.y
            + vertex_positions[2] * uv.barycentric_coords.z;
        
        const Vec3f normal = (uv.transform * Vec4f((vertex_normals[0] * uv.barycentric_coords.x
            + vertex_normals[1] * uv.barycentric_coords.y
            + vertex_normals[2] * uv.barycentric_coords.z), 0.0f)).GetXYZ().Normalize();

        out_rays.PushBack(LightmapRay {
            Ray {
                position,
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
    }
}

void LightmapJob::TraceSingleRayOnCPU(const LightmapRay &ray, LightmapRayHitPayload &out_payload)
{
    out_payload.throughput = Vec4f(0.0f);
    out_payload.emissive = Vec4f(0.0f);
    out_payload.radiance = Vec4f(0.0f);
    out_payload.normal = Vec3f(0.0f);
    out_payload.distance = -1.0f;
    out_payload.barycentric_coords = Vec3f(0.0f);
    out_payload.mesh_id = ID<Mesh>::invalid;
    out_payload.triangle_index = 0;

    if (!m_acceleration_structure) {
        HYP_LOG(Lightmap, LogLevel::WARNING, "No CPU acceleration structure set while tracing on CPU, cannot perform trace");

        return;
    }

    LightmapRayTestResults results = m_acceleration_structure->TestRay(ray.ray);
    
    if (!results.Any()) {
        return;
    }
    
    for (const Pair<float, LightmapRayHitData> &hit_data : results) {
        if (hit_data.first < 0.0f) {
            continue;
        }

        if (!hit_data.second.entity.IsValid()) {
            continue;
        }

        auto lightmap_entity_it = m_all_entities_map->Find(hit_data.second.entity);

        if (lightmap_entity_it == m_all_entities_map->End() || lightmap_entity_it->second == nullptr) {
            continue;
        }

        const LightmapEntity &lightmap_entity = *lightmap_entity_it->second;

        const ID<Mesh> mesh_id = lightmap_entity.mesh.GetID();

        const Vec3f barycentric_coords = hit_data.second.hit.barycentric_coords;

        const Triangle &triangle = hit_data.second.triangle;

        const Vec2f uv = triangle.GetPoint(0).GetTexCoord0() * barycentric_coords.x
            + triangle.GetPoint(1).GetTexCoord0() * barycentric_coords.y
            + triangle.GetPoint(2).GetTexCoord0() * barycentric_coords.z;

        const Vec4f color = Vec4f(lightmap_entity.material->GetParameter(Material::MATERIAL_KEY_ALBEDO));

        // @TODO sample textures

        out_payload.emissive = Vec4f(0.0f);
        out_payload.throughput = color;
        out_payload.barycentric_coords = barycentric_coords;
        out_payload.mesh_id = mesh_id;
        out_payload.triangle_index = hit_data.second.hit.id;
        out_payload.normal = hit_data.second.hit.normal;
        out_payload.distance = hit_data.first;

        return;
    }
}

void LightmapJob::TraceRaysOnCPU(const Array<LightmapRay> &rays, LightmapShadingType shading_type)
{
    m_current_tasks.Concat(TaskSystem::GetInstance().ParallelForEach_Async(rays, [this, shading_type](const LightmapRay &first_ray, uint index, uint batch_index)
    {
        uint32 seed = (uint32)rand();//index * m_texel_index;

        FixedArray<LightmapRay, max_bounces_cpu + 1> rays;
        
        FixedArray<LightmapRayHitPayload, max_bounces_cpu + 1> bounces;
        int num_bounces = 0;

        Vec3f direction = first_ray.ray.direction;

        if (shading_type == LightmapShadingType::LIGHTMAP_SHADING_TYPE_IRRADIANCE) {
            direction = MathUtil::RandomInHemisphere(
                Vec3f(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed)),
                first_ray.ray.direction
            );
        }

        Vec3f origin = first_ray.ray.position + first_ray.ray.direction * 0.05f;

        for (int bounce_index = 0; bounce_index < max_bounces_cpu; bounce_index++) {
            LightmapRay bounce_ray = first_ray;

            if (bounce_index != 0) {
                bounce_ray.mesh_id = bounces[bounce_index - 1].mesh_id;
                bounce_ray.triangle_index = bounces[bounce_index - 1].triangle_index;
            }

            bounce_ray.ray = Ray {
                origin,
                direction
            };

            rays[bounce_index] = bounce_ray;

            LightmapRayHitPayload &payload = bounces[bounce_index];
            payload.throughput = Vec4f(1.0f);
            payload.emissive = Vec4f(0.0f);
            payload.radiance = Vec4f(0.0f);
            payload.distance = -1.0f;
            payload.normal = Vec3f(0.0f);
            payload.barycentric_coords = Vec3f(0.0f);
            payload.mesh_id = ID<Mesh>::invalid;
            payload.triangle_index = 0;

            TraceSingleRayOnCPU(bounce_ray, payload);

            if (payload.distance < 0.0f) {
                // @TODO Sample environment map
                const Vec3f normal = bounce_index == 0 ? first_ray.ray.direction : bounces[bounce_index - 1].normal;

                // // testing!! @FIXME
                // const Vec3f L = Vec3f(-0.4f, 0.65f, 0.1f).Normalize();
                // payload.emissive += Vec4f(1.0f) * MathUtil::Max(0.0f, normal.Dot(L));

                payload.emissive += Vec4f(1.0f);

                ++num_bounces;

                break;
            }

            Vec3f hit_position = origin + direction * payload.distance;
            origin = hit_position + payload.normal * 0.05f;

            if (shading_type == LightmapShadingType::LIGHTMAP_SHADING_TYPE_IRRADIANCE) {
                direction = MathUtil::RandomInHemisphere(
                    Vec3f(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed)),
                    payload.normal
                );
            } else {
                // @TODO
            }

            ++num_bounces;
        }

        for (int bounce_index = int(num_bounces - 1); bounce_index >= 0; bounce_index--) {
            Vec4f radiance = bounces[bounce_index].emissive;

            if (bounce_index != num_bounces - 1) {
                radiance += bounces[bounce_index + 1].radiance * bounces[bounce_index].throughput;
            }

            float p = MathUtil::Max(radiance.x, MathUtil::Max(radiance.y, MathUtil::Max(radiance.z, radiance.w)));

            if (MathUtil::RandomFloat(seed) > p) {
                break;
            }

            radiance /= MathUtil::Max(p, 0.0001f);

            bounces[bounce_index].radiance = radiance;
        }

        if (num_bounces != 0) {
            LightmapHit hits;
            hits.color = bounces[0].radiance;

            if (MathUtil::IsNaN(hits.color) || !MathUtil::IsFinite(hits.color)) {
                HYP_LOG_ONCE(Lightmap, LogLevel::WARNING, "NaN or infinite color detected while tracing rays");

                hits.color = Vec4f(0.0f);
            }

            hits.color.w = 1.0f;

            IntegrateRayHits(rays.Data(), &hits, 1, shading_type);
        }
    }));
}

#pragma endregion LightmapJob

#pragma region LightmapRenderer

LightmapRenderer::LightmapRenderer(Name name)
    : RenderComponent(name),
      m_trace_mode(LIGHTMAP_TRACE_MODE_CPU),
      m_num_jobs { 0 }
{
}

void LightmapRenderer::Init()
{
    AssertThrowMsg(m_num_jobs.Get(MemoryOrder::ACQUIRE) == 0, "Cannot initialize lightmap renderer -- jobs currently running!");

    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool()) {
        // trace on GPU if the card supports ray tracing
        m_trace_mode = LIGHTMAP_TRACE_MODE_GPU;
    }

    static constexpr uint ideal_triangles_per_job = 10000;

    // Build jobs
    GetParent()->GetScene()->GetEntityManager()->PushCommand([this](EntityManager &mgr, GameCounter::TickUnit)
    {
        HYP_LOG(Lightmap, LogLevel::INFO, "Building graph for lightmapper");

        m_lightmap_entities.Clear();
        m_all_entities_map.Clear();

        for (auto [entity, mesh_component, transform_component, bounding_box_component] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ)) {
            if (!mesh_component.mesh.IsValid()) {
                HYP_LOG(Lightmap, LogLevel::INFO, "Skip entity with invalid mesh on MeshComponent");

                continue;
            }

            if (!mesh_component.material.IsValid()) {
                HYP_LOG(Lightmap, LogLevel::INFO, "Skip entity with invalid material on MeshComponent");

                continue;
            }

            // Only process opaque and translucent materials
            if (mesh_component.material->GetBucket() != BUCKET_OPAQUE && mesh_component.material->GetBucket() != BUCKET_TRANSLUCENT) {
                HYP_LOG(Lightmap, LogLevel::INFO, "Skip entity with bucket that is not opaque or translucent");

                continue;
            }

            m_lightmap_entities.PushBack(LightmapEntity {
                entity,
                mesh_component.mesh,
                mesh_component.material,
                transform_component.transform,
                bounding_box_component.world_aabb
            });
        }

        UniquePtr<LightmapTopLevelAccelerationStructure> acceleration_structure;

        uint num_triangles = 0;

        SizeType lightmap_entities_index_start;
        SizeType lightmap_entities_index_end;

        for (lightmap_entities_index_start = 0, lightmap_entities_index_end = 0; lightmap_entities_index_end < m_lightmap_entities.Size(); lightmap_entities_index_end++) {
            LightmapEntity &lightmap_entity = m_lightmap_entities[lightmap_entities_index_end];

            m_all_entities_map.Set(lightmap_entity.entity_id, &lightmap_entity);

            if (ideal_triangles_per_job != 0 && num_triangles != 0 && num_triangles + lightmap_entity.mesh->NumIndices() / 3 > ideal_triangles_per_job) {
                if (lightmap_entities_index_end - lightmap_entities_index_start != 0) {
                    HYP_LOG(Lightmap, LogLevel::INFO, "Adding lightmap job for {} entities", lightmap_entities_index_end - lightmap_entities_index_start);

                    UniquePtr<LightmapJob> job(new LightmapJob(m_trace_mode, m_parent->GetScene(), m_lightmap_entities.ToSpan().Slice(lightmap_entities_index_start, lightmap_entities_index_end), &m_all_entities_map, std::move(acceleration_structure)));

                    lightmap_entities_index_start = lightmap_entities_index_end;

                    AddJob(std::move(job));
                }

                num_triangles = 0;
            }

            if (m_trace_mode == LIGHTMAP_TRACE_MODE_CPU) {
                if (!acceleration_structure) {
                    acceleration_structure.Reset(new LightmapTopLevelAccelerationStructure);
                }

                acceleration_structure->Add(UniquePtr<LightmapBottomLevelAccelerationStructure>(
                    new LightmapBottomLevelAccelerationStructure(
                        lightmap_entity.entity_id,
                        lightmap_entity.mesh,
                        lightmap_entity.transform
                    )
                ));
            }

            HYP_LOG(Lightmap, LogLevel::INFO, "Add Entity (#{}) to be processed for lightmap", lightmap_entity.entity_id.Value());

            num_triangles += lightmap_entity.mesh->NumIndices() / 3;
        }

        if (lightmap_entities_index_end - lightmap_entities_index_start != 0) {
            HYP_LOG(Lightmap, LogLevel::INFO, "Adding final lightmap job for {} entities", lightmap_entities_index_end - lightmap_entities_index_start);

            UniquePtr<LightmapJob> job(new LightmapJob(m_trace_mode, m_parent->GetScene(), m_lightmap_entities.ToSpan().Slice(lightmap_entities_index_start, lightmap_entities_index_end), &m_all_entities_map, std::move(acceleration_structure)));

            AddJob(std::move(job));
        } else {
            HYP_LOG(Lightmap, LogLevel::INFO, "Skipping adding lightmap job, no entities to process");
        }
    });
}

void LightmapRenderer::InitGame()
{ 
}

void LightmapRenderer::OnRemoved()
{
    m_path_tracer_radiance.Reset();
    m_path_tracer_irradiance.Reset();

    Mutex::Guard guard(m_queue_mutex);

    m_queue.Clear();

    m_num_jobs.Set(0, MemoryOrder::RELEASE);
}

void LightmapRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    uint32 num_jobs = m_num_jobs.Get(MemoryOrder::ACQUIRE);

    if (!num_jobs) {
        return;
    }

    DebugLog(
        LogType::Debug,
        "Processing %u lightmap jobs...\n",
        num_jobs
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

    job->Update();
}

void LightmapRenderer::OnRender(Frame *frame)
{
    const uint frame_index = frame->GetFrameIndex();
    const uint previous_frame_index = (frame_index + max_frames_in_flight - 1) % max_frames_in_flight;

    // Do nothing if not in GPU trace mode
    if (m_trace_mode != LIGHTMAP_TRACE_MODE_GPU) {
        return;
    }

    if (!m_num_jobs.Get(MemoryOrder::ACQUIRE)) {
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
        if (!m_num_jobs.Get(MemoryOrder::ACQUIRE)) {
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

    // for (auto &bitmap : bitmaps) {
    //     Bitmap<4, float> bitmap_dilated = bitmap;

    //     // Dilate lightmap
    //     for (uint x = 0; x < bitmap.GetWidth(); x++) {
    //         for (uint y = 0; y < bitmap.GetHeight(); y++) {
    //             Vec3f color = bitmap.GetPixel(x, y);
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x - 1, y - 1));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x, y - 1));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x + 1, y - 1));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x - 1, y));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x + 1, y));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x - 1, y + 1));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x, y + 1));
    //             color = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) > 0.0f ? color : Vec3f(bitmap.GetPixel(x + 1, y + 1));

    //             bitmap_dilated.GetPixel(x, y) = color;
    //         }
    //     }

    //     bitmap = bitmap_dilated;

    //     // Apply bilateral blur
    //     Bitmap<4, float> bitmap_blurred = bitmap;

    //     for (uint x = 0; x < bitmap.GetWidth(); x++) {
    //         for (uint y = 0; y < bitmap.GetHeight(); y++) {
    //             Vec3f color = Vec3f(0.0f);

    //             float total_weight = 0.0f;

    //             for (int dx = -1; dx <= 1; dx++) {
    //                 for (int dy = -1; dy <= 1; dy++) {
    //                     const uint nx = x + dx;
    //                     const uint ny = y + dy;

    //                     if (nx >= bitmap.GetWidth() || ny >= bitmap.GetHeight()) {
    //                         continue;
    //                     }

    //                     const Vec3f neighbor_color = bitmap.GetPixel(nx, ny);

    //                     const float spatial_weight = MathUtil::Exp(-float(dx * dx + dy * dy) / (2.0f * 1.0f));
    //                     const float color_weight = MathUtil::Exp(-(neighbor_color - bitmap.GetPixel(x, y)).LengthSquared() / (2.0f * 0.1f));

    //                     const float weight = spatial_weight * color_weight;

    //                     color += neighbor_color * weight;
    //                     total_weight += weight;
    //                 }
    //             }

    //             bitmap_blurred.GetPixel(x, y) = color / MathUtil::Max(total_weight, MathUtil::epsilon_f);
    //         }
    //     }

    //     bitmap = bitmap_blurred;
    // }
    
    // Temp; write to rgb8 bitmap
    uint num = rand() % 150;
    bitmaps[0].Write("lightmap_" + String::ToString(num) + "_radiance.bmp");
    bitmaps[1].Write("lightmap_" + String::ToString(num) + "_irradiance.bmp");

    FixedArray<Handle<Texture>, 2> textures;

    for (uint i = 0; i < 2; i++) {
        RC<StreamedTextureData> streamed_data(new StreamedTextureData(TextureData {
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA32F,
                Extent3D { uv_map.width, uv_map.height, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_REPEAT
            },
            bitmaps[i].ToByteBuffer()
        }));

        Handle<Texture> texture = CreateObject<Texture>(std::move(streamed_data));
        InitObject(texture);

        textures[i] = std::move(texture);
    }

    for (LightmapEntity &lightmap_entity : job->GetEntities()) {
        bool is_new_material = false;

        if (!lightmap_entity.material) {
            // @TODO: Set to default material
            continue;
        }

        if (!lightmap_entity.material->IsDynamic()) {
            lightmap_entity.material = lightmap_entity.material->Clone();

            is_new_material = true;
        }
        
        lightmap_entity.material->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_RADIANCE_MAP, textures[0]);
        lightmap_entity.material->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_IRRADIANCE_MAP, textures[1]);

        if (is_new_material) {
            InitObject(lightmap_entity.material);

            GetParent()->GetScene()->GetEntityManager()->PushCommand([entity = lightmap_entity.entity_id, mesh = lightmap_entity.mesh, new_material = lightmap_entity.material](EntityManager &mgr, GameCounter::TickUnit)
            {
                if (MeshComponent *mesh_component = mgr.TryGetComponent<MeshComponent>(entity)) {
                    mesh_component->material = std::move(new_material);
                    mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
                } else {
                    mgr.AddComponent<MeshComponent>(entity, MeshComponent {
                        mesh,
                        new_material
                    });
                }
            });
        }        
    }

    m_queue.Pop();
    m_num_jobs.Decrement(1, MemoryOrder::RELEASE);
}

#pragma endregion LightmapRenderer

} // namespace hyperion