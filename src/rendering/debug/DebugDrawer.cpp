/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/debug/DebugDrawer.hpp>
#include <rendering/EnvProbe.hpp>
#include <util/MeshBuilder.hpp>
#include <Engine.hpp>

namespace hyperion {

DebugDrawer::DebugDrawer()
{
    m_draw_commands.Reserve(256);
}

DebugDrawer::~DebugDrawer()
{
}

void DebugDrawer::Create()
{
    m_shapes[uint(DebugDrawShape::SPHERE)] = MeshBuilder::NormalizedCubeSphere(8);
    m_shapes[uint(DebugDrawShape::BOX)] = MeshBuilder::Cube();
    m_shapes[uint(DebugDrawShape::PLANE)] = MeshBuilder::Quad();

    for (auto &shape : m_shapes) {
        InitObject(shape);
    }

    m_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(DebugAABB),
        ShaderProperties(
            static_mesh_vertex_attributes,
            Array<String> { "IMMEDIATE_MODE" }
        )
    );

    AssertThrow(InitObject(m_shader));

    renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);
    AssertThrow(descriptor_table != nullptr);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &debug_drawer_descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(DebugDrawerDescriptorSet), frame_index);
        AssertThrow(debug_drawer_descriptor_set != nullptr);

        debug_drawer_descriptor_set->SetElement(HYP_NAME(ImmediateDrawsBuffer), g_engine->GetRenderData()->immediate_draws.GetBuffer(frame_index));
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_render_group = g_engine->CreateRenderGroup(
        m_shader,
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = static_mesh_vertex_attributes
            },
            MaterialAttributes {
                .bucket         = Bucket::BUCKET_OPAQUE,
                .fill_mode      = FillMode::FILL,
                .blend_function = BlendFunction::None(),
                .cull_faces     = FaceCullMode::NONE
            }
        ),
        std::move(descriptor_table)
    );

    if (m_render_group) {
        g_engine->GetDeferredSystem()
            .Get(m_render_group->GetRenderableAttributes().GetMaterialAttributes().bucket)
            .AddFramebuffersToRenderGroup(m_render_group);

        InitObject(m_render_group);
    }
}

void DebugDrawer::Destroy()
{
    m_shapes = { };

    m_render_group.Reset();
    m_shader.Reset();
}

void DebugDrawer::Render(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (m_num_draw_commands_pending_addition.load(std::memory_order_relaxed) != 0) {
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

        const ImmediateDrawShaderData shader_data {
            draw_command.transform_matrix,
            draw_command.color.Packed(),
            env_probe_type,
            env_probe_id.Value()
        };

        g_engine->GetRenderData()->immediate_draws.Set(index, shader_data);
    }

    g_engine->GetRenderData()->immediate_draws.UpdateBuffer(
        g_engine->GetGPUDevice(),
        frame->GetFrameIndex()
    );

    RendererProxy proxy = m_render_group->GetProxy();
    proxy.Bind(frame);

    const uint debug_drawer_descriptor_set_index = proxy.GetGraphicsPipeline()->GetDescriptorTable().Get()->GetDescriptorSetIndex(HYP_NAME(DebugDrawerDescriptorSet));

    proxy.GetGraphicsPipeline()->GetDescriptorTable().Get()->Bind<GraphicsPipelineRef>(
        proxy.GetCommandBuffer(frame_index),
        frame_index,
        proxy.GetGraphicsPipeline(),
        {
            {
                HYP_NAME(DebugDrawerDescriptorSet),
                {
                    { HYP_NAME(ImmediateDrawsBuffer), HYP_RENDER_OBJECT_OFFSET(ImmediateDraw, 0) }
                }
            },
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                }
            },
            {
                HYP_NAME(Object),
                {
#ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
                    { HYP_NAME(MaterialsBuffer), HYP_RENDER_OBJECT_OFFSET(Material, 0) },
#endif
                    { HYP_NAME(SkeletonsBuffer), HYP_RENDER_OBJECT_OFFSET(Skeleton, 0) },
                    { HYP_NAME(EntityInstanceBatchesBuffer), 0 }
                }
            }
        }
    );

    for (SizeType index = 0; index < m_draw_commands.Size(); index++) {
        const DebugDrawCommand &draw_command = m_draw_commands[index];

        proxy.GetGraphicsPipeline()->GetDescriptorTable().Get()->GetDescriptorSet(HYP_NAME(DebugDrawerDescriptorSet), frame_index)->Bind(
            proxy.GetCommandBuffer(frame_index),
            proxy.GetGraphicsPipeline(),
            {
                { HYP_NAME(ImmediateDrawsBuffer), HYP_RENDER_OBJECT_OFFSET(ImmediateDraw, index) }
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
    std::lock_guard guard(m_draw_commands_mutex);

    SizeType size = m_draw_commands_pending_addition.Size();
    int64 previous_value = m_num_draw_commands_pending_addition.fetch_sub(int64(size), std::memory_order_relaxed);
    AssertThrow(previous_value - int64(size) >= 0);

    m_draw_commands.Concat(std::move(m_draw_commands_pending_addition));
}

UniquePtr<DebugDrawCommandList> DebugDrawer::CreateCommandList()
{
    return UniquePtr<DebugDrawCommandList>(new DebugDrawCommandList(this));
}

void DebugDrawer::CommitCommands(DebugDrawCommandList &command_list)
{
    std::lock_guard guard(m_draw_commands_mutex);

    m_num_draw_commands_pending_addition.fetch_add(int64(command_list.m_draw_commands.Size()), std::memory_order_relaxed);
    m_draw_commands_pending_addition.Concat(std::move(command_list.m_draw_commands));
}

void DebugDrawer::Sphere(const Vec3f &position, float radius, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        DebugDrawType::DEFAULT,
        Transform(position, radius, Quaternion::Identity()).GetMatrix(),
        color
    });
}

void DebugDrawer::AmbientProbeSphere(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id)
{
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
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::BOX,
        DebugDrawType::DEFAULT,
        Transform(position, size, Quaternion::Identity()).GetMatrix(),
        color
    });
}

void DebugDrawer::Plane(const FixedArray<Vec3f, 4> &points, Color color)
{
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

// command list methods

void DebugDrawCommandList::Sphere(const Vec3f &position, float radius, Color color)
{
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
    Mutex::Guard guard(m_draw_commands_mutex);
    
    m_debug_drawer->CommitCommands(*this);
}

} // namespace hyperion
