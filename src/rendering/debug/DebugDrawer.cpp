/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/debug/DebugDrawer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/EnvGrid.hpp>
#include <rendering/RenderProbe.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/UIRenderer.hpp>

#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <scene/Mesh.hpp>
#include <scene/Scene.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIText.hpp>

#include <util/MeshBuilder.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region DebugDrawCommand_Probe

struct DebugDrawCommand_Probe : DebugDrawCommand
{
    ID<EnvProbe>    env_probe_id;
};

#pragma endregion DebugDrawCommand_Probe

#pragma region DebugDrawCommand_UI

struct DebugDrawCommand_UI : DebugDrawCommand
{
    const HypClass  *ui_object_type;
    Vec2f           position;
    String          text;
};

#pragma endregion DebugDrawCommand_UI

#pragma region MeshDebugDrawShapeBase

MeshDebugDrawShapeBase::MeshDebugDrawShapeBase(IDebugDrawCommandList &command_list, const Handle<Mesh> &mesh)
    : m_command_list(command_list),
      m_mesh(mesh)
{
    if (InitObject(mesh)) {
        mesh->SetPersistentRenderResourceEnabled(true);
    }
}

#pragma endregion MeshDebugDrawShapeBase

#pragma region SphereDebugDrawShape

SphereDebugDrawShape::SphereDebugDrawShape(IDebugDrawCommandList &command_list)
    : MeshDebugDrawShapeBase(command_list, MeshBuilder::NormalizedCubeSphere(4))
{
}

void SphereDebugDrawShape::operator()(const Vec3f &position, float radius, const Color &color)
{
    m_command_list.Push(MakeUnique<DebugDrawCommand>(DebugDrawCommand {
        this,
        Transform(position, radius, Quaternion::Identity()).GetMatrix(),
        color
    }));
}

#pragma endregion SphereDebugDrawShape

#pragma region AmbientProbeDebugDrawShape

void AmbientProbeDebugDrawShape::operator()(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id)
{
    UniquePtr<DebugDrawCommand_Probe> command = MakeUnique<DebugDrawCommand_Probe>();
    command->shape = this;
    command->transform_matrix = Transform(position, radius, Quaternion::Identity()).GetMatrix();
    command->color = Color::White();
    command->env_probe_id = env_probe_id;

    m_command_list.Push(std::move(command));
}

#pragma endregion AmbientProbeDebugDrawShape

#pragma region ReflectionProbeDebugDrawShape

void ReflectionProbeDebugDrawShape::operator()(const Vec3f &position, float radius, ID<EnvProbe> env_probe_id)
{
    UniquePtr<DebugDrawCommand_Probe> command = MakeUnique<DebugDrawCommand_Probe>();
    command->shape = this;
    command->transform_matrix = Transform(position, radius, Quaternion::Identity()).GetMatrix();
    command->color = Color::White();
    command->env_probe_id = env_probe_id;

    m_command_list.Push(std::move(command));
}

#pragma endregion ReflectionProbeDebugDrawShape

#pragma region BoxDebugDrawShape

BoxDebugDrawShape::BoxDebugDrawShape(IDebugDrawCommandList &command_list)
    : MeshDebugDrawShapeBase(command_list, MeshBuilder::Cube())
{
}

void BoxDebugDrawShape::operator()(const Vec3f &position, const Vec3f &size, const Color &color)
{
    m_command_list.Push(MakeUnique<DebugDrawCommand>(DebugDrawCommand {
        this,
        Transform(position, size, Quaternion::Identity()).GetMatrix(),
        color
    }));
}

#pragma endregion BoxDebugDrawShape

#pragma region PlaneDebugDrawShape

PlaneDebugDrawShape::PlaneDebugDrawShape(IDebugDrawCommandList &command_list)
    : MeshDebugDrawShapeBase(command_list, MeshBuilder::Quad())
{
}

void PlaneDebugDrawShape::operator()(const FixedArray<Vec3f, 4> &points, const Color &color)
{
    Vec3f x = (points[1] - points[0]).Normalize();
    Vec3f y = (points[2] - points[0]).Normalize();
    Vec3f z = x.Cross(y).Normalize();

    const Vec3f center = points.Avg();

    Matrix4 transform_matrix;
    transform_matrix.rows[0] = Vec4f(x, 0.0f);
    transform_matrix.rows[1] = Vec4f(y, 0.0f);
    transform_matrix.rows[2] = Vec4f(z, 0.0f);
    transform_matrix.rows[3] = Vec4f(center, 1.0f);

    m_command_list.Push(MakeUnique<DebugDrawCommand>(DebugDrawCommand {
        this,
        transform_matrix,
        color
    }));
}

#pragma endregion PlaneDebugDrawShape

#pragma region Text2DDebugDrawShape

Text2DDebugDrawShape::Text2DDebugDrawShape(IDebugDrawCommandList &command_list)
    : m_command_list(command_list)
{
}

void Text2DDebugDrawShape::operator()(const Vec2f &position, const String &text, const Color &color)
{
    UniquePtr<DebugDrawCommand_UI> command = MakeUnique<DebugDrawCommand_UI>();
    command->shape = this;
    command->position = position;
    command->text = text;
    command->color = color;
    command->ui_object_type = UIText::Class();

    m_command_list.Push(std::move(command));
}

#pragma endregion Text2DDebugDrawShape

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
    Threads::AssertOnThread(g_render_thread);

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

    AssertThrow(mesh != nullptr);
    AssertThrow(mesh->IsReady());

    CommandBuffer *command_buffer = GetCommandBuffer(frame);
    AssertThrow(command_buffer != nullptr);

    mesh->GetRenderResource().Render(command_buffer);
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

#pragma region UIDebugDrawer

class UIDebugDrawer : public IDebugDrawer
{
public:
    UIDebugDrawer();
    virtual ~UIDebugDrawer() override = default;

    virtual void Initialize() override;
    virtual void Update(GameCounter::TickUnit delta) override;
    virtual void Render(Frame *frame) override;

    void HandleDrawCommand(const DebugDrawCommand &draw_command);

private:
    Handle<RenderEnvironment>   m_environment;
    RC<UIRenderer>              m_ui_renderer;
    RC<UIStage>                 m_ui_stage;
};

UIDebugDrawer::UIDebugDrawer()
{
}

void UIDebugDrawer::Initialize()
{
    HYP_SCOPE;

    m_environment = CreateObject<RenderEnvironment>();
    InitObject(m_environment);

    m_ui_stage = MakeRefCountedPtr<UIStage>(g_game_thread);
    m_ui_stage->Init();

    m_ui_renderer = m_environment->AddRenderSubsystem<UIRenderer>(Name::Unique("UIDebugDrawer"), m_ui_stage);
}

void UIDebugDrawer::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    m_ui_stage->Update(delta);

    m_environment->Update(delta);
}

void UIDebugDrawer::Render(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    //m_ui_renderer->Render(frame, nullptr);
}

void UIDebugDrawer::HandleDrawCommand(const DebugDrawCommand &draw_command)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    AssertThrow(draw_command.shape != nullptr);
    AssertThrow(draw_command.shape->GetDebugDrawType() == DebugDrawType::UI);

    const DebugDrawCommand_UI &ui_command = static_cast<const DebugDrawCommand_UI &>(draw_command);

    Proc<void> command_proc;

    if (ui_command.ui_object_type == UIText::Class()) {
        command_proc = [ui_stage = m_ui_stage, ui_command]()
        {
            // @TEMP; Refactor -- needs to not create a new text object every frame, and should not create a managed object.
            RC<UIText> element = ui_stage->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            element->SetText(ui_command.text);
            element->SetTextColor(ui_command.color);
            ui_stage->AddChildUIObject(element);
        };
    }

    if (command_proc.IsValid()) {
        Threads::GetThread(g_game_thread)->GetScheduler().Enqueue(std::move(command_proc), TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

#pragma endregion UIDebugDrawer

#pragma region DebugDrawer

DebugDrawer::DebugDrawer()
    : m_default_command_list(*this),
      m_ui_debug_drawer(MakeUnique<UIDebugDrawer>()),
      m_is_initialized(false),
      Sphere(m_default_command_list.Sphere),
      AmbientProbe(m_default_command_list.AmbientProbe),
      ReflectionProbe(m_default_command_list.ReflectionProbe),
      Box(m_default_command_list.Box),
      Plane(m_default_command_list.Plane),
      Text2D(m_default_command_list.Text2D)
{
    m_draw_commands.Reserve(256);
}

DebugDrawer::~DebugDrawer()
{
    m_render_group.Reset();
    m_shader.Reset();

    SafeRelease(std::move(m_instance_buffers));
}

void DebugDrawer::Initialize()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    AssertThrow(!m_is_initialized.Get(MemoryOrder::ACQUIRE));

    m_ui_debug_drawer->Initialize();

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_instance_buffers[frame_index] = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
        DeferCreate(m_instance_buffers[frame_index], g_engine->GetGPUDevice(), m_draw_commands.Capacity() * sizeof(ImmediateDrawShaderData));
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

    const uint32 debug_drawer_descriptor_set_index = descriptor_table->GetDescriptorSetIndex(NAME("DebugDrawerDescriptorSet"));
    AssertThrow(debug_drawer_descriptor_set_index != ~0u);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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

    m_is_initialized.Set(true, MemoryOrder::RELEASE);
}

void DebugDrawer::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    m_ui_debug_drawer->Update(delta);
}

void DebugDrawer::Render(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    // wait for initialization on the game thread
    if (!m_is_initialized.Get(MemoryOrder::ACQUIRE)) {
        return;
    }

    if (!m_default_command_list.IsEmpty()) {
        CommitCommands(m_default_command_list);
    }

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

    const uint32 frame_index = frame->GetFrameIndex();

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const CameraRenderResource *camera_render_resource = &g_engine->GetRenderState()->GetActiveCamera();

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
        const DebugDrawCommand &draw_command = *m_draw_commands[index];

        ID<EnvProbe> env_probe_id = ID<EnvProbe>::invalid;
        uint32 env_probe_type = uint32(ENV_PROBE_TYPE_INVALID);

        if (draw_command.shape == &AmbientProbe) {
            const DebugDrawCommand_Probe &probe_command = static_cast<const DebugDrawCommand_Probe &>(draw_command);
            env_probe_id = probe_command.env_probe_id;
            env_probe_type = uint32(ENV_PROBE_TYPE_AMBIENT);
        } else if (draw_command.shape == &ReflectionProbe) {
            const DebugDrawCommand_Probe &probe_command = static_cast<const DebugDrawCommand_Probe &>(draw_command);
            env_probe_id = probe_command.env_probe_id;
            env_probe_type = uint32(ENV_PROBE_TYPE_REFLECTION);
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

    const uint32 debug_drawer_descriptor_set_index = proxy.GetGraphicsPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("DebugDrawerDescriptorSet"));
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
                        { NAME("ImmediateDrawsBuffer"), ShaderDataOffset<ImmediateDrawShaderData>(0) }
                    }
                },
                {
                    NAME("Scene"),
                    {
                        { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                        { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource) },
                        { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(g_engine->GetRenderState()->bound_env_grid.ToIndex()) },
                        { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(g_engine->GetRenderState()->GetActiveEnvProbe().GetID().ToIndex()) }
                    }
                },
                {
                    NAME("Object"),
                    {
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
                        { NAME("ImmediateDrawsBuffer"), ShaderDataOffset<ImmediateDrawShaderData>(0) }
                    }
                },
                {
                    NAME("Scene"),
                    {
                        { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                        { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource) },
                        { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(g_engine->GetRenderState()->bound_env_grid.ToIndex()) },
                        { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(g_engine->GetRenderState()->GetActiveEnvProbe().GetID().ToIndex()) }
                    }
                },
                {
                    NAME("Object"),
                    {
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

    bool any_ui_objects = false;

    for (SizeType index = 0; index < m_draw_commands.Size(); index++) {
        const DebugDrawCommand &draw_command = *m_draw_commands[index];

        proxy.GetGraphicsPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("DebugDrawerDescriptorSet"), frame_index)->Bind(
            proxy.GetCommandBuffer(frame),
            proxy.GetGraphicsPipeline(),
            {
                { NAME("ImmediateDrawsBuffer"), ShaderDataOffset<ImmediateDrawShaderData>(index) }
            },
            debug_drawer_descriptor_set_index
        );

        switch (draw_command.shape->GetDebugDrawType()) {
        case DebugDrawType::MESH:
        {
            MeshDebugDrawShapeBase *mesh_shape = static_cast<MeshDebugDrawShapeBase *>(draw_command.shape);
            proxy.DrawMesh(frame, mesh_shape->GetMesh().Get());
            break;
        }
        case DebugDrawType::UI:
        {
            m_ui_debug_drawer->HandleDrawCommand(draw_command);

            any_ui_objects = true;

            break;
        }
        }
    }

    proxy.Submit(frame);

    m_draw_commands.Clear();

    if (any_ui_objects) {
        m_ui_debug_drawer->Render(frame);
    }
}

void DebugDrawer::UpdateDrawCommands()
{
    HYP_SCOPE;

    Mutex::Guard guard(m_draw_commands_mutex);

    SizeType size = m_draw_commands_pending_addition.Size();
    int64 previous_value = int64(m_num_draw_commands_pending_addition.Decrement(uint32(size), MemoryOrder::ACQUIRE_RELEASE));
    AssertThrow(previous_value - int64(size) >= 0);

    m_draw_commands.Concat(std::move(m_draw_commands_pending_addition));
}

UniquePtr<DebugDrawCommandList> DebugDrawer::CreateCommandList()
{
    HYP_SCOPE;

    return MakeUnique<DebugDrawCommandList>(*this);
}

void DebugDrawer::CommitCommands(DebugDrawCommandList &command_list)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_draw_commands_mutex);

    const SizeType num_added_items = command_list.m_draw_commands.Size();
    m_draw_commands_pending_addition.Concat(std::move(command_list.m_draw_commands));
    m_num_draw_commands_pending_addition.Increment(uint32(num_added_items), MemoryOrder::RELEASE);
}

#pragma endregion DebugDrawer

#pragma region DebugDrawCommandList

void DebugDrawCommandList::Push(UniquePtr<DebugDrawCommand> &&command)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_draw_commands_mutex);

    m_draw_commands.PushBack(std::move(command));

    m_num_draw_commands.Increment(1, MemoryOrder::RELEASE);
}

void DebugDrawCommandList::Commit()
{
    HYP_SCOPE;
    
    Mutex::Guard guard(m_draw_commands_mutex);

    const SizeType num_added_items = m_draw_commands.Size();
    
    m_debug_drawer.CommitCommands(*this);

    m_num_draw_commands.Decrement(uint32(num_added_items), MemoryOrder::RELEASE);
}

#pragma endregion DebugDrawCommandList

} // namespace hyperion
