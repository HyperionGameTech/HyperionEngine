#include <rendering/debug/DebugDrawer.hpp>
#include <util/MeshBuilder.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::DynamicStorageBufferDescriptor;

struct RENDER_COMMAND(CreateImmediateModeDescriptors) : renderer::RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateImmediateModeDescriptors)(FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets)
        : descriptor_sets(std::move(descriptor_sets))
    {
    }

    virtual Result operator()()
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Create(
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

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

    uint index = 0;
    for (auto &shape : m_shapes) {
        DebugLog(LogType::Debug, "init object at %u\n", index++);
        AssertThrow(InitObject(shape));
    }

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_descriptor_sets[frame_index] = MakeRenderObject<renderer::DescriptorSet>();

        m_descriptor_sets[frame_index]
            ->AddDescriptor<DynamicStorageBufferDescriptor>(0)
            ->SetElementBuffer<ImmediateDrawShaderData>(0, Engine::Get()->GetRenderData()->immediate_draws.GetBuffer());
    }

    PUSH_RENDER_COMMAND(CreateImmediateModeDescriptors, m_descriptor_sets);

    m_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(DebugAABB),
        ShaderProperties(
            renderer::static_mesh_vertex_attributes,
            Array<String> { "IMMEDIATE_MODE" }
        )
    );

    Engine::Get()->InitObject(m_shader);

    m_renderer_instance = Engine::Get()->CreateRenderGroup(
        m_shader,
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .fill_mode = FillMode::FILL,
                .blend_mode = BlendMode::NONE,
                .cull_faces = FaceCullMode::NONE
            }
        ),
        Array<DescriptorSetRef> {
            m_descriptor_sets[0],
            Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL),
            Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        }
    );
    
    if (m_renderer_instance) {
        Engine::Get()->GetDeferredSystem()
            .Get(m_renderer_instance->GetRenderableAttributes().GetMaterialAttributes().bucket)
            .AddFramebuffersToRenderGroup(m_renderer_instance);

        Engine::Get()->InitObject(m_renderer_instance);
    }
}

void DebugDrawer::Destroy()
{
    m_shapes = { };

    m_renderer_instance.Reset();
    m_shader.Reset();
    
    SafeRelease(std::move(m_descriptor_sets));
}

void DebugDrawer::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (m_num_draw_commands_pending_addition.load(std::memory_order_relaxed) != 0) {
        UpdateDrawCommands();
    }

    if (m_draw_commands.Empty()) {
        return;
    }

    if (!m_renderer_instance) {
        m_draw_commands.Clear();

        return;
    }

    const uint frame_index = frame->GetFrameIndex();

    for (SizeType index = 0; index < m_draw_commands.Size(); index++) {
        const auto &draw_command = m_draw_commands[index];

        const ImmediateDrawShaderData shader_data {
            draw_command.transform_matrix,
            draw_command.color.Packed()
        };

        Engine::Get()->GetRenderData()->immediate_draws.Set(index, shader_data);
    }

    Engine::Get()->GetRenderData()->immediate_draws.UpdateBuffer(
        Engine::Get()->GetGPUDevice(),
        frame->GetFrameIndex()
    );

    RendererProxy proxy = m_renderer_instance->GetProxy();
    proxy.Bind(frame);

    proxy.GetCommandBuffer(frame->GetFrameIndex())->BindDescriptorSets(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        proxy.GetGraphicsPipeline(),
        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::Index(1), DescriptorSet::Index(2) },
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, Engine::Get()->GetRenderState().GetScene().id.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Light, 0),
            HYP_RENDER_OBJECT_OFFSET(EnvGrid, Engine::Get()->GetRenderState().bound_env_grid.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(EnvProbe, Engine::Get()->GetRenderState().GetActiveEnvProbe().ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Camera, Engine::Get()->GetRenderState().GetCamera().id.ToIndex())
        }
    );

    for (SizeType index = 0; index < m_draw_commands.Size(); index++) {
        const DebugDrawCommand &draw_command = m_draw_commands[index];

        proxy.GetCommandBuffer(frame_index)->BindDescriptorSet(
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
            proxy.GetGraphicsPipeline(),
            m_descriptor_sets[frame_index],
            0,
            FixedArray<uint32, 1> { uint32(index * sizeof(ImmediateDrawShaderData)) }
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

void DebugDrawer::CommitCommands(DebugDrawCommandList &&command_list)
{
    std::lock_guard guard(m_draw_commands_mutex);
    
    m_num_draw_commands_pending_addition.fetch_add(int64(command_list.m_draw_commands.Size()), std::memory_order_relaxed);
    m_draw_commands_pending_addition.Concat(std::move(command_list.m_draw_commands));
}

void DebugDrawer::Sphere(const Vector3 &position, float radius, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        Transform(position, radius, Quaternion::identity).GetMatrix(),
        color
    });
}

void DebugDrawer::Box(const Vector3 &position, const Vector3 &size, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::BOX,
        Transform(position, size, Quaternion::identity).GetMatrix(),
        color
    });
}

void DebugDrawer::Plane(const FixedArray<Vector3, 4> &points, Color color)
{
    Vector3 x = (points[1] - points[0]).Normalize();
    Vector3 y = (points[2] - points[0]).Normalize();
    Vector3 z = x.Cross(y).Normalize();

    const Vector3 center = points.Avg();

    Matrix4 transform_matrix;
    transform_matrix.rows[0] = Vector4(x, 0.0f);
    transform_matrix.rows[1] = Vector4(y, 0.0f);
    transform_matrix.rows[2] = Vector4(z, 0.0f);
    transform_matrix.rows[3] = Vector4(center, 1.0f);

    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::PLANE,
        transform_matrix,
        color
    });
}

// command list methods

void DebugDrawCommandList::Sphere(const Vector3 &position, float radius, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        Transform(position, radius, Quaternion::identity).GetMatrix(),
        color
    });
}

void DebugDrawCommandList::Box(const Vector3 &position, const Vector3 &size, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::BOX,
        Transform(position, size, Quaternion::identity).GetMatrix(),
        color
    });
}

void DebugDrawCommandList::Plane(const Vector3 &position, const Vector2 &size, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::PLANE,
        Transform(position, Vector3(size.x, size.y, 1.0f), Quaternion::identity).GetMatrix(),
        color
    });
}

void DebugDrawCommandList::Commit()
{
    m_debug_drawer->CommitCommands(std::move(*this));
}

} // namespace hyperion::v2