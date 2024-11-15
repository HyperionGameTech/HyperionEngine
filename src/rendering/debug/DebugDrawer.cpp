/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/debug/DebugDrawer.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>

#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <util/MeshBuilder.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region DebugDrawerRenderGroupProxy

class DebugDrawerRenderGroupProxy
{
public:
    DebugDrawerRenderGroupProxy(RenderGroup *render_group)
        : m_render_group(render_group)
    {
    }

    const CommandBufferRef &GetCommandBuffer(Frame *frame) const;

    const GraphicsPipelineRef &GetGraphicsPipeline() const;

    /*! \brief For using this RenderGroup as a standalone graphics pipeline that will simply
        be bound, with all draw calls recorded elsewhere. */
    void Bind(Frame *frame);

    /*! \brief For using this RenderGroup as a standalone graphics pipeline that will simply
        be bound, with all draw calls recorded elsewhere. */
    void DrawMesh(Frame *frame, Mesh *mesh);

    /*! \brief For using this RenderGroup as a standalone graphics pipeline that will simply
        be bound, with all draw calls recorded elsewhere. */
    void Submit(Frame *frame);

private:
    RenderGroup *m_render_group;
};

const CommandBufferRef &DebugDrawerRenderGroupProxy::GetCommandBuffer(Frame *frame) const
{
    if (m_render_group->m_command_buffers != nullptr) {
        return (*m_render_group->m_command_buffers)[frame->GetFrameIndex()].Front();
    }

    return frame->GetCommandBuffer();
}

const GraphicsPipelineRef &DebugDrawerRenderGroupProxy::GetGraphicsPipeline() const
{
    return m_render_group->m_pipeline;
}

void DebugDrawerRenderGroupProxy::Bind(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    CommandBuffer *command_buffer = GetCommandBuffer(frame);
    AssertThrow(command_buffer != nullptr);

    if (command_buffer != frame->GetCommandBuffer()) {
        command_buffer->Begin(g_engine->GetGPUDevice(), m_render_group->m_pipeline->GetRenderPass());
    }

    m_render_group->m_pipeline->Bind(command_buffer);
}

void DebugDrawerRenderGroupProxy::DrawMesh(Frame *frame, Mesh *mesh)
{
    HYP_SCOPE;

    CommandBuffer *command_buffer = GetCommandBuffer(frame);
    AssertThrow(command_buffer != nullptr);

    mesh->Render(command_buffer);
}

void DebugDrawerRenderGroupProxy::Submit(Frame *frame)
{
    HYP_SCOPE;

    CommandBuffer *command_buffer = GetCommandBuffer(frame);
    AssertThrow(command_buffer != nullptr);

    if (command_buffer != frame->GetCommandBuffer()) {
        command_buffer->End(g_engine->GetGPUDevice());
        command_buffer->SubmitSecondary(frame->GetCommandBuffer());
    }
}

#pragma endregion DebugDrawerRenderGroupProxy

#pragma region DebugDrawer

DebugDrawer::DebugDrawer()
{
    m_draw_commands.Reserve(256);
}

DebugDrawer::~DebugDrawer()
{
    m_render_group.Reset();
    m_shader.Reset();

    SafeRelease(std::move(m_instance_buffers));
}

void DebugDrawer::Create()
{
    HYP_SCOPE;

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_instance_buffers[frame_index] = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
        DeferCreate(m_instance_buffers[frame_index], g_engine->GetGPUDevice(), m_draw_commands.Capacity() * sizeof(ImmediateDrawShaderData));
    }

    m_shapes[uint(DebugDrawShape::SPHERE)] = MeshBuilder::NormalizedCubeSphere(4);
    m_shapes[uint(DebugDrawShape::BOX)] = MeshBuilder::Cube();
    m_shapes[uint(DebugDrawShape::PLANE)] = MeshBuilder::Quad();

    for (auto &shape : m_shapes) {
        InitObject(shape);
    }

    m_shader = g_shader_manager->GetOrCreate(
        NAME("DebugAABB"),
        ShaderProperties(
            static_mesh_vertex_attributes,
            Array<String> { "IMMEDIATE_MODE" }
        )
    );

    AssertThrow(m_shader.IsValid());

    renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);
    AssertThrow(descriptor_table != nullptr);

    const uint debug_drawer_descriptor_set_index = descriptor_table->GetDescriptorSetIndex(NAME("DebugDrawerDescriptorSet"));
    AssertThrow(debug_drawer_descriptor_set_index != ~0u);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &debug_drawer_descriptor_set = descriptor_table->GetDescriptorSet(debug_drawer_descriptor_set_index, frame_index);
        AssertThrow(debug_drawer_descriptor_set != nullptr);

        debug_drawer_descriptor_set->SetElement(NAME("ImmediateDrawsBuffer"), m_instance_buffers[frame_index]);
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_render_group = CreateObject<RenderGroup>(
        m_shader,
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = static_mesh_vertex_attributes
            },
            MaterialAttributes {
                .bucket         = Bucket::BUCKET_TRANSLUCENT,
                .fill_mode      = FillMode::FILL,
                .blend_function = BlendFunction::None(),
                .cull_faces     = FaceCullMode::NONE
            }
        ),
        std::move(descriptor_table),
        RenderGroupFlags::DEFAULT
    );

    const FramebufferRef &framebuffer = g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(m_render_group->GetRenderableAttributes().GetMaterialAttributes().bucket).GetFramebuffer();
    m_render_group->AddFramebuffer(framebuffer);

    InitObject(m_render_group);
}

void DebugDrawer::Render(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (m_num_draw_commands_pending_addition.Get(MemoryOrder::ACQUIRE) != 0) {
        UpdateDrawCommands();
    }

    if (m_draw_commands.Empty()) {
        return;
    }

    if (!m_render_group) {
        m_draw_commands.Clear();

        return;
    }

    const uint frame_index = frame->GetFrameIndex();

    GPUBufferRef &instance_buffer = m_instance_buffers[frame_index];
    bool was_instance_buffer_rebuilt = false;

    if (m_draw_commands.Size() * sizeof(ImmediateDrawShaderData) > instance_buffer->Size()) {
        HYPERION_ASSERT_RESULT(instance_buffer->EnsureCapacity(
            g_engine->GetGPUDevice(),
            m_draw_commands.Size() * sizeof(ImmediateDrawShaderData),
            &was_instance_buffer_rebuilt
        ));
    }

    Array<ImmediateDrawShaderData> shader_data;
    shader_data.Resize(m_draw_commands.Size());

    for (SizeType index = 0; index < m_draw_commands.Size(); index++) {
        const auto &draw_command = m_draw_commands[index];

        ID<EnvProbe> env_probe_id = ID<EnvProbe>::invalid;
        uint32 env_probe_type = uint32(ENV_PROBE_TYPE_INVALID);

        switch (draw_command.type) {
        case DebugDrawType::AMBIENT_PROBE:
            env_probe_type = uint32(ENV_PROBE_TYPE_AMBIENT);
            env_probe_id = draw_command.env_probe_id;
            break;
        case DebugDrawType::REFLECTION_PROBE:
            env_probe_type = uint32(ENV_PROBE_TYPE_REFLECTION);
            env_probe_id = draw_command.env_probe_id;
            break;
        default:
            break;
        }

       shader_data[index] = ImmediateDrawShaderData {
            draw_command.transform_matrix,
            draw_command.color.Packed(),
            env_probe_type,
            env_probe_id.Value()
        };
    }

    instance_buffer->Copy(
        g_engine->GetGPUDevice(),
        shader_data.ByteSize(),
        shader_data.Data()
    );

    DebugDrawerRenderGroupProxy proxy(m_render_group.Get());
    proxy.Bind(frame);

    const uint debug_drawer_descriptor_set_index = proxy.GetGraphicsPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("DebugDrawerDescriptorSet"));
    AssertThrow(debug_drawer_descriptor_set_index != ~0u);

    // Update descriptor set if instance buffer was rebuilt
    if (was_instance_buffer_rebuilt) {
        const DescriptorSetRef &descriptor_set = proxy.GetGraphicsPipeline()->GetDescriptorTable()->GetDescriptorSet(debug_drawer_descriptor_set_index, frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("ImmediateDrawsBuffer"), instance_buffer);

        descriptor_set->Update(g_engine->GetGPUDevice());
    }

    if (renderer::RenderConfig::ShouldCollectUniqueDrawCallPerMaterial()) {
        proxy.GetGraphicsPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
            proxy.GetCommandBuffer(frame),
            frame_index,
            proxy.GetGraphicsPipeline(),
            {
                {
                    NAME("DebugDrawerDescriptorSet"),
                    {
                        { NAME("ImmediateDrawsBuffer"), HYP_RENDER_OBJECT_OFFSET(ImmediateDraw, 0) }
                    }
                },
                {
                    NAME("Scene"),
                    {
                        { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                        { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                        { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                        { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                        { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                    }
                },
                {
                    NAME("Object"),
                    {
                        { NAME("MaterialsBuffer"), HYP_RENDER_OBJECT_OFFSET(Material, 0) },
                        { NAME("SkeletonsBuffer"), HYP_RENDER_OBJECT_OFFSET(Skeleton, 0) }
                    }
                },
                {
                    NAME("Instancing"),
                    {
                        { NAME("EntityInstanceBatchesBuffer"), 0 }
                    }
                }
            }
        );
    } else {
        proxy.GetGraphicsPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
            proxy.GetCommandBuffer(frame),
            frame_index,
            proxy.GetGraphicsPipeline(),
            {
                {
                    NAME("DebugDrawerDescriptorSet"),
                    {
                        { NAME("ImmediateDrawsBuffer"), HYP_RENDER_OBJECT_OFFSET(ImmediateDraw, 0) }
                    }
                },
                {
                    NAME("Scene"),
                    {
                        { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                        { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                        { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                        { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                        { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                    }
                },
                {
                    NAME("Object"),
                    {
                        { NAME("SkeletonsBuffer"), HYP_RENDER_OBJECT_OFFSET(Skeleton, 0) }
                    }
                },
                {
                    NAME("Instancing"),
                    {
                        { NAME("EntityInstanceBatchesBuffer"), 0 }
                    }
                }
            }
        );
    }

    for (SizeType index = 0; index < m_draw_commands.Size(); index++) {
        const DebugDrawCommand &draw_command = m_draw_commands[index];

        proxy.GetGraphicsPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("DebugDrawerDescriptorSet"), frame_index)->Bind(
            proxy.GetCommandBuffer(frame),
            proxy.GetGraphicsPipeline(),
            {
                { NAME("ImmediateDrawsBuffer"), HYP_RENDER_OBJECT_OFFSET(ImmediateDraw, index) }
            },
            debug_drawer_descriptor_set_index
        );

        proxy.DrawMesh(frame, m_shapes[uint(draw_command.shape)].Get());
    }

    proxy.Submit(frame);

    m_draw_commands.Clear();
}

void DebugDrawer::UpdateDrawCommands()
{
    HYP_SCOPE;

    Mutex::Guard guard(m_draw_commands_mutex);

    SizeType size = m_draw_commands_pending_addition.Size();
    int64 previous_value = m_num_draw_commands_pending_addition.Decrement(int64(size), MemoryOrder::ACQUIRE_RELEASE);
    AssertThrow(previous_value - int64(size) >= 0);

    m_draw_commands.Concat(std::move(m_draw_commands_pending_addition));
}

UniquePtr<DebugDrawCommandList> DebugDrawer::CreateCommandList()
{
    HYP_SCOPE;

    return MakeUnique<DebugDrawCommandList>(this);
}

void DebugDrawer::CommitCommands(DebugDrawCommandList &command_list)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_draw_commands_mutex);

    const SizeType num_added_items = command_list.m_draw_commands.Size();
    m_draw_commands_pending_addition.Concat(std::move(command_list.m_draw_commands));
    m_num_draw_commands_pending_addition.Increment(int64(num_added_items), MemoryOrder::RELEASE);
}

void DebugDrawer::Sphere(const Vec3f &position, float radius, Color color)
{
    HYP_SCOPE;

    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        DebugDrawType::DEFAULT,
        Transform(position, radius, Quaternion::Identity()).GetMatrix(),
        color
    });
}

void DebugDrawer::AmbientProbeSphere(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id)
{
    HYP_SCOPE;

    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        DebugDrawType::AMBIENT_PROBE,
        Transform(position, radius, Quaternion::Identity()).GetMatrix(),
        Color { },
        env_probe_id
    });
}

void DebugDrawer::ReflectionProbeSphere(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id)
{
    HYP_SCOPE;

    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        DebugDrawType::REFLECTION_PROBE,
        Transform(position, radius, Quaternion::Identity()).GetMatrix(),
        Color { },
        env_probe_id
    });
}

void DebugDrawer::Box(const Vec3f &position, const Vec3f &size, Color color)
{
    HYP_SCOPE;

    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::BOX,
        DebugDrawType::DEFAULT,
        Transform(position, size, Quaternion::Identity()).GetMatrix(),
        color
    });
}

void DebugDrawer::Plane(const FixedArray<Vec3f, 4> &points, Color color)
{
    HYP_SCOPE;

    Vec3f x = (points[1] - points[0]).Normalize();
    Vec3f y = (points[2] - points[0]).Normalize();
    Vec3f z = x.Cross(y).Normalize();

    const Vec3f center = points.Avg();

    Matrix4 transform_matrix;
    transform_matrix.rows[0] = Vector4(x, 0.0f);
    transform_matrix.rows[1] = Vector4(y, 0.0f);
    transform_matrix.rows[2] = Vector4(z, 0.0f);
    transform_matrix.rows[3] = Vector4(center, 1.0f);

    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::PLANE,
        DebugDrawType::DEFAULT,
        transform_matrix,
        color
    });
}

#pragma endregion DebugDrawer

#pragma region DebugDrawCommandList

void DebugDrawCommandList::Sphere(const Vec3f &position, float radius, Color color)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_draw_commands_mutex);
    
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        DebugDrawType::DEFAULT,
        Transform(position, radius, Quaternion::Identity()).GetMatrix(),
        color
    });
}

void DebugDrawCommandList::Box(const Vec3f &position, const Vec3f &size, Color color)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_draw_commands_mutex);

    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::BOX,
        DebugDrawType::DEFAULT,
        Transform(position, size, Quaternion::Identity()).GetMatrix(),
        color
    });
}

void DebugDrawCommandList::Plane(const Vec3f &position, const Vector2 &size, Color color)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_draw_commands_mutex);

    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::PLANE,
        DebugDrawType::DEFAULT,
        Transform(position, Vec3f(size.x, size.y, 1.0f), Quaternion::Identity()).GetMatrix(),
        color
    });
}

void DebugDrawCommandList::Commit()
{
    HYP_SCOPE;
    
    Mutex::Guard guard(m_draw_commands_mutex);
    
    m_debug_drawer->CommitCommands(*this);
}

#pragma endregion DebugDrawCommandList

} // namespace hyperion
