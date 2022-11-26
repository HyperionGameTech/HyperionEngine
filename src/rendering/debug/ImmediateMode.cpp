#include <rendering/debug/ImmediateMode.hpp>
#include <util/MeshBuilder.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::DynamicStorageBufferDescriptor;

struct RENDER_COMMAND(CreateImmediateModeDescriptors) : RenderCommandBase2
{
    UniquePtr<renderer::DescriptorSet> *descriptor_sets;

    RENDER_COMMAND(CreateImmediateModeDescriptors)(UniquePtr<renderer::DescriptorSet> *descriptor_sets)
        : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Create(
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

ImmediateMode::ImmediateMode()
{
    m_draw_commands.Reserve(256);
}

ImmediateMode::~ImmediateMode()
{
}

void ImmediateMode::Create()
{
    m_shapes[UInt(DebugDrawShape::SPHERE)] = Engine::Get()->CreateObject<Mesh>(MeshBuilder::NormalizedCubeSphere(8));
    m_shapes[UInt(DebugDrawShape::BOX)] = Engine::Get()->CreateObject<Mesh>(MeshBuilder::Cube());
    m_shapes[UInt(DebugDrawShape::PLANE)] = Engine::Get()->CreateObject<Mesh>(MeshBuilder::Quad());

    for (auto &shape : m_shapes) {
        AssertThrow(Engine::Get()->InitObject(shape));
    }

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_descriptor_sets[frame_index] = UniquePtr<DescriptorSet>::Construct();

        m_descriptor_sets[frame_index]
            ->AddDescriptor<DynamicStorageBufferDescriptor>(0)
            ->SetElementBuffer<ImmediateDrawShaderData>(0, Engine::Get()->GetRenderData()->immediate_draws.GetBuffer(frame_index).get());
    }

    RenderCommands::Push<RENDER_COMMAND(CreateImmediateModeDescriptors)>(m_descriptor_sets.Data());

    m_shader = Engine::Get()->CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader(
        "DebugAABB",
        ShaderProps(
            renderer::static_mesh_vertex_attributes,
            Array<String> { "IMMEDIATE_MODE" }
        )
    ));

    Engine::Get()->InitObject(m_shader);

    m_renderer_instance = Engine::Get()->CreateRendererInstance(
        m_shader,
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .fill_mode = FillMode::LINE,
                .cull_faces = FaceCullMode::NONE,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING
            }
        ),
        Array<const DescriptorSet *> {
            m_descriptor_sets[0].Get(),
            Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL),
            Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        }
    );
    
    if (m_renderer_instance) {
        Engine::Get()->GetDeferredSystem()
            .Get(m_renderer_instance->GetRenderableAttributes().material_attributes.bucket)
            .AddFramebuffersToPipeline(m_renderer_instance);

        Engine::Get()->InitObject(m_renderer_instance);
    }
}

void ImmediateMode::Destroy()
{
    m_shapes = { };

    m_renderer_instance.Reset();
    m_shader.Reset();

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        Engine::Get()->SafeRelease(std::move(m_descriptor_sets[frame_index]));
    }
}

void ImmediateMode::Render(
    
    Frame *frame
)
{
    if (m_num_draw_commands_pending_additon.load(std::memory_order_relaxed) != 0) {
        UpdateDrawCommands();
    }

    if (m_draw_commands.Empty()) {
        return;
    }

    if (!m_renderer_instance) {
        m_draw_commands.Clear();

        return;
    }

    const UInt frame_index = frame->GetFrameIndex();

    for (SizeType index = 0; index < m_draw_commands.Size(); index++) {
        const auto &draw_command = m_draw_commands[index];

        const ImmediateDrawShaderData shader_data {
            draw_command.transform.GetMatrix(),
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
            UInt32(Engine::Get()->render_state.GetScene().id.ToIndex() * sizeof(SceneShaderData)),
            HYP_RENDER_OBJECT_OFFSET(Light, 0)
        }
    );

    for (SizeType index = 0; index < m_draw_commands.Size(); index++) {
        const DebugDrawCommand &draw_command = m_draw_commands[index];

        proxy.GetCommandBuffer(frame_index)->BindDescriptorSet(
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
            proxy.GetGraphicsPipeline(),
            m_descriptor_sets[frame_index].Get(),
            0,
            FixedArray<UInt32, 1> { UInt32(index * sizeof(ImmediateDrawShaderData)) }
        );

        proxy.DrawMesh(frame, m_shapes[UInt(draw_command.shape)].Get());
    }

    proxy.Submit(frame);

    m_draw_commands.Clear();
}

void ImmediateMode::UpdateDrawCommands()
{
    std::lock_guard guard(m_draw_commands_mutex);

    SizeType size = m_draw_commands_pending_addition.Size();
    Int64 previous_value = m_num_draw_commands_pending_additon.fetch_sub(Int64(size), std::memory_order_relaxed);
    AssertThrow(previous_value - Int64(size) >= 0);

    m_draw_commands.Concat(std::move(m_draw_commands_pending_addition));
}

void ImmediateMode::CommitCommands(DebugDrawCommandList &&command_list)
{
    std::lock_guard guard(m_draw_commands_mutex);
    
    m_num_draw_commands_pending_additon.fetch_add(Int64(command_list.m_draw_commands.Size()), std::memory_order_relaxed);
    m_draw_commands_pending_addition.Concat(std::move(command_list.m_draw_commands));
}

void ImmediateMode::Sphere(const Vector3 &position, Float radius, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        Transform(position, radius, Quaternion::identity),
        color
    });
}

void ImmediateMode::Box(const Vector3 &position, const Vector3 &size, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::BOX,
        Transform(position, size, Quaternion::identity),
        color
    });
}

void ImmediateMode::Plane(const Vector3 &position, const Vector2 &size, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::PLANE,
        Transform(position, Vector3(size, 1.0f), Quaternion::identity),
        color
    });
}

// command list methods

void DebugDrawCommandList::Sphere(const Vector3 &position, Float radius, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        Transform(position, radius, Quaternion::identity),
        color
    });
}

void DebugDrawCommandList::Box(const Vector3 &position, const Vector3 &size, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::BOX,
        Transform(position, size, Quaternion::identity),
        color
    });
}

void DebugDrawCommandList::Plane(const Vector3 &position, const Vector2 &size, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::PLANE,
        Transform(position, Vector3(size, 1.0f), Quaternion::identity),
        color
    });
}

void DebugDrawCommandList::Commit()
{
    m_immediate_mode->CommitCommands(std::move(*this));
}

} // namespace hyperion::v2