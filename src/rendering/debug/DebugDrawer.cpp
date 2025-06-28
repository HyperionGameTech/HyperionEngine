/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/debug/DebugDrawer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

#include <rendering/UIRenderer.hpp>

#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <scene/Mesh.hpp>
#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>

#include <util/MeshBuilder.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

static RenderableAttributeSet GetRenderableAttributes()
{
    return RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes },
        MaterialAttributes {
            .bucket = RB_TRANSLUCENT,
            .fill_mode = FM_FILL,
            .blend_function = BlendFunction::None(),
            .flags = MaterialAttributeFlags::DEPTH_TEST,
            .stencil_function = StencilFunction {
                .pass_op = SO_KEEP,
                .fail_op = SO_KEEP,
                .depth_fail_op = SO_KEEP,
                .compare_op = SCO_NOT_EQUAL,
                .mask = 0x0,
                .value = 0x1 } });
}

#pragma region DebugDrawCommand_Probe

struct DebugDrawCommand_Probe : DebugDrawCommand
{
    TResourceHandle<RenderEnvProbe> env_probe_resource_handle;
};

#pragma endregion DebugDrawCommand_Probe

#pragma region MeshDebugDrawShapeBase

MeshDebugDrawShapeBase::MeshDebugDrawShapeBase(IDebugDrawCommandList& command_list, const Handle<Mesh>& mesh)
    : m_command_list(command_list),
      m_mesh(mesh)
{
    if (InitObject(mesh))
    {
        mesh->SetPersistentRenderResourceEnabled(true);
    }
}

#pragma endregion MeshDebugDrawShapeBase

#pragma region SphereDebugDrawShape

SphereDebugDrawShape::SphereDebugDrawShape(IDebugDrawCommandList& command_list)
    : MeshDebugDrawShapeBase(command_list, MeshBuilder::NormalizedCubeSphere(4))
{
}

void SphereDebugDrawShape::operator()(const Vec3f& position, float radius, const Color& color)
{
    (*this)(position, radius, color, GetRenderableAttributes());
}

void SphereDebugDrawShape::operator()(const Vec3f& position, float radius, const Color& color, const RenderableAttributeSet& attributes)
{
    m_command_list.Push(MakeUnique<DebugDrawCommand>(DebugDrawCommand {
        this,
        Transform(position, radius, Quaternion::Identity()).GetMatrix(),
        color,
        attributes }));
}

#pragma endregion SphereDebugDrawShape

#pragma region AmbientProbeDebugDrawShape

void AmbientProbeDebugDrawShape::operator()(const Vec3f& position, float radius, const EnvProbe& env_probe)
{
    AssertThrow(env_probe.IsReady());

    UniquePtr<DebugDrawCommand_Probe> command = MakeUnique<DebugDrawCommand_Probe>();
    command->shape = this;
    command->transform_matrix = Transform(position, radius, Quaternion::Identity()).GetMatrix();
    command->color = Color::White();
    command->env_probe_resource_handle = TResourceHandle<RenderEnvProbe>(env_probe.GetRenderResource());

    m_command_list.Push(std::move(command));
}

#pragma endregion AmbientProbeDebugDrawShape

#pragma region ReflectionProbeDebugDrawShape

void ReflectionProbeDebugDrawShape::operator()(const Vec3f& position, float radius, const EnvProbe& env_probe)
{
    AssertThrow(env_probe.IsReady());

    UniquePtr<DebugDrawCommand_Probe> command = MakeUnique<DebugDrawCommand_Probe>();
    command->shape = this;
    command->transform_matrix = Transform(position, radius, Quaternion::Identity()).GetMatrix();
    command->color = Color::White();
    command->env_probe_resource_handle = TResourceHandle<RenderEnvProbe>(env_probe.GetRenderResource());

    m_command_list.Push(std::move(command));
}

#pragma endregion ReflectionProbeDebugDrawShape

#pragma region BoxDebugDrawShape

BoxDebugDrawShape::BoxDebugDrawShape(IDebugDrawCommandList& command_list)
    : MeshDebugDrawShapeBase(command_list, MeshBuilder::Cube())
{
}

void BoxDebugDrawShape::operator()(const Vec3f& position, const Vec3f& size, const Color& color)
{
    (*this)(position, size, color, GetRenderableAttributes());
}

void BoxDebugDrawShape::operator()(const Vec3f& position, const Vec3f& size, const Color& color, const RenderableAttributeSet& attributes)
{
    m_command_list.Push(MakeUnique<DebugDrawCommand>(DebugDrawCommand {
        this,
        Transform(position, size, Quaternion::Identity()).GetMatrix(),
        color,
        attributes }));
}

#pragma endregion BoxDebugDrawShape

#pragma region PlaneDebugDrawShape

PlaneDebugDrawShape::PlaneDebugDrawShape(IDebugDrawCommandList& command_list)
    : MeshDebugDrawShapeBase(command_list, MeshBuilder::Quad())
{
}

void PlaneDebugDrawShape::operator()(const FixedArray<Vec3f, 4>& points, const Color& color)
{
    (*this)(points, color, GetRenderableAttributes());
}

void PlaneDebugDrawShape::operator()(const FixedArray<Vec3f, 4>& points, const Color& color, const RenderableAttributeSet& attributes)
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
        color,
        attributes }));
}

#pragma endregion PlaneDebugDrawShape

#pragma region DebugDrawer

DebugDrawer::DebugDrawer()
    : m_config(DebugDrawerConfig::FromConfig()),
      m_default_command_list(*this),
      m_is_initialized(false),
      Sphere(m_default_command_list.Sphere),
      AmbientProbe(m_default_command_list.AmbientProbe),
      ReflectionProbe(m_default_command_list.ReflectionProbe),
      Box(m_default_command_list.Box),
      Plane(m_default_command_list.Plane),
      m_num_draw_commands_pending_addition(0)
{
    m_draw_commands.Reserve(256);
}

DebugDrawer::~DebugDrawer()
{
    m_shader.Reset();

    SafeRelease(std::move(m_instance_buffers));
    SafeRelease(std::move(m_descriptor_table));
}

void DebugDrawer::Initialize()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    AssertThrow(!m_is_initialized.Get(MemoryOrder::ACQUIRE));

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        m_instance_buffers[frame_index] = g_rendering_api->MakeGPUBuffer(GPUBufferType::STORAGE_BUFFER, m_draw_commands.Capacity() * sizeof(ImmediateDrawShaderData));
        DeferCreate(m_instance_buffers[frame_index]);
    }

    m_shader = g_shader_manager->GetOrCreate(
        NAME("DebugAABB"),
        ShaderProperties(
            static_mesh_vertex_attributes,
            Array<String> { "IMMEDIATE_MODE" }));

    AssertThrow(m_shader.IsValid());

    const DescriptorTableDeclaration& descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    m_descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);
    AssertThrow(m_descriptor_table != nullptr);

    const uint32 debug_drawer_descriptor_set_index = m_descriptor_table->GetDescriptorSetIndex(NAME("DebugDrawerDescriptorSet"));
    AssertThrow(debug_drawer_descriptor_set_index != ~0u);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& debug_drawer_descriptor_set = m_descriptor_table->GetDescriptorSet(debug_drawer_descriptor_set_index, frame_index);
        AssertThrow(debug_drawer_descriptor_set != nullptr);

        debug_drawer_descriptor_set->SetElement(NAME("ImmediateDrawsBuffer"), m_instance_buffers[frame_index]);
    }

    DeferCreate(m_descriptor_table);

    m_is_initialized.Set(true, MemoryOrder::RELEASE);
}

void DebugDrawer::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (!IsEnabled())
    {
        return;
    }
}

void DebugDrawer::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    // wait for initialization on the game thread
    if (!m_is_initialized.Get(MemoryOrder::ACQUIRE))
    {
        return;
    }

    if (!m_default_command_list.IsEmpty())
    {
        m_default_command_list.Commit();
    }

    if (m_num_draw_commands_pending_addition.Get(MemoryOrder::ACQUIRE) != 0)
    {
        UpdateDrawCommands();
    }

    if (!IsEnabled())
    {
        return;
    }

    if (m_draw_commands.Empty())
    {
        return;
    }

    const uint32 frame_index = frame->GetFrameIndex();

    GPUBufferRef& instance_buffer = m_instance_buffers[frame_index];
    bool was_instance_buffer_rebuilt = false;

    if (m_draw_commands.Size() * sizeof(ImmediateDrawShaderData) > instance_buffer->Size())
    {
        HYPERION_ASSERT_RESULT(instance_buffer->EnsureCapacity(
            m_draw_commands.Size() * sizeof(ImmediateDrawShaderData),
            &was_instance_buffer_rebuilt));
    }

    const uint32 debug_drawer_descriptor_set_index = m_descriptor_table->GetDescriptorSetIndex(NAME("DebugDrawerDescriptorSet"));
    AssertThrow(debug_drawer_descriptor_set_index != ~0u);

    const DescriptorSetRef& debug_drawer_descriptor_set = m_descriptor_table->GetDescriptorSet(debug_drawer_descriptor_set_index, frame_index);
    AssertThrow(debug_drawer_descriptor_set != nullptr);

    // Update descriptor set if instance buffer was rebuilt
    if (was_instance_buffer_rebuilt)
    {
        debug_drawer_descriptor_set->SetElement(NAME("ImmediateDrawsBuffer"), instance_buffer);

        debug_drawer_descriptor_set->Update();
    }

    Array<ImmediateDrawShaderData> shader_data;
    shader_data.Resize(m_draw_commands.Size());

    for (SizeType index = 0; index < m_draw_commands.Size(); index++)
    {
        const DebugDrawCommand& draw_command = *m_draw_commands[index];

        uint32 env_probe_type = uint32(EPT_INVALID);
        uint32 env_probe_index = ~0u;

        if (draw_command.shape == &AmbientProbe)
        {
            const DebugDrawCommand_Probe& probe_command = static_cast<const DebugDrawCommand_Probe&>(draw_command);
            env_probe_type = uint32(EPT_AMBIENT);
            env_probe_index = probe_command.env_probe_resource_handle->GetBufferIndex();
        }
        else if (draw_command.shape == &ReflectionProbe)
        {
            const DebugDrawCommand_Probe& probe_command = static_cast<const DebugDrawCommand_Probe&>(draw_command);
            env_probe_type = uint32(EPT_REFLECTION);
            env_probe_index = probe_command.env_probe_resource_handle->GetBufferIndex();
        }

        shader_data[index] = ImmediateDrawShaderData {
            draw_command.transform_matrix,
            draw_command.color.Packed(),
            env_probe_type,
            env_probe_index
        };
    }

    instance_buffer->Copy(shader_data.ByteSize(), shader_data.Data());

    struct
    {
        HashCode attributes_hash_code;
        GraphicsPipelineRef graphics_pipeline;
        uint32 drawable_layer = ~0u;
    } previous_state;

    for (SizeType index = 0; index < m_draw_commands.Size(); index++)
    {
        const DebugDrawCommand& draw_command = *m_draw_commands[index];

        bool is_new_graphics_pipeline = false;

        GraphicsPipelineRef& graphics_pipeline = previous_state.graphics_pipeline;

        if (!graphics_pipeline.IsValid() || previous_state.attributes_hash_code != draw_command.attributes.GetHashCode())
        {
            graphics_pipeline = FetchGraphicsPipeline(draw_command.attributes, ++previous_state.drawable_layer);
            previous_state.attributes_hash_code = draw_command.attributes.GetHashCode();

            is_new_graphics_pipeline = true;
        }

        if (is_new_graphics_pipeline)
        {
            frame->GetCommandList().Add<BindGraphicsPipeline>(graphics_pipeline);

            frame->GetCommandList().Add<BindDescriptorTable>(
                m_descriptor_table,
                graphics_pipeline,
                ArrayMap<Name, ArrayMap<Name, uint32>> {
                    { NAME("DebugDrawerDescriptorSet"),
                        { { NAME("ImmediateDrawsBuffer"), ShaderDataOffset<ImmediateDrawShaderData>(0) } } },
                    { NAME("Global"),
                        { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                            { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) },
                            { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(render_setup.env_grid, 0) } } },
                    { NAME("Object"),
                        {} },
                    { NAME("Instancing"),
                        { { NAME("EntityInstanceBatchesBuffer"), 0 } } } },
                frame_index);
        }

        frame->GetCommandList().Add<BindDescriptorSet>(
            debug_drawer_descriptor_set,
            graphics_pipeline,
            ArrayMap<Name, uint32> {
                { NAME("ImmediateDrawsBuffer"), ShaderDataOffset<ImmediateDrawShaderData>(index) } },
            debug_drawer_descriptor_set_index);

        switch (draw_command.shape->GetDebugDrawType())
        {
        case DebugDrawType::MESH:
        {
            MeshDebugDrawShapeBase* mesh_shape = static_cast<MeshDebugDrawShapeBase*>(draw_command.shape);

            mesh_shape->GetMesh()->GetRenderResource().Render(frame->GetCommandList());

            break;
        }
        default:
            HYP_UNREACHABLE();
        }
    }

    m_draw_commands.Clear();
}

void DebugDrawer::UpdateDrawCommands()
{
    HYP_SCOPE;

    Mutex::Guard guard(m_draw_commands_mutex);

    SizeType size = m_draw_commands_pending_addition.Size();
    int64 previous_value = int64(m_num_draw_commands_pending_addition.Decrement(uint32(size), MemoryOrder::ACQUIRE_RELEASE));
    AssertThrow(previous_value - int64(size) >= 0);

    m_draw_commands.Concat(std::move(m_draw_commands_pending_addition));

    AssertThrow(m_draw_commands_pending_addition.Empty());
}

UniquePtr<DebugDrawCommandList> DebugDrawer::CreateCommandList()
{
    HYP_SCOPE;

    return MakeUnique<DebugDrawCommandList>(*this);
}

void DebugDrawer::CommitCommands(DebugDrawCommandList& command_list)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_draw_commands_mutex);

    const SizeType num_added_items = command_list.m_draw_commands.Size();
    m_num_draw_commands_pending_addition.Increment(uint32(num_added_items), MemoryOrder::RELEASE);
    m_draw_commands_pending_addition.Concat(std::move(command_list.m_draw_commands));
}

GraphicsPipelineRef DebugDrawer::FetchGraphicsPipeline(RenderableAttributeSet attributes, uint32 drawable_layer)
{
    HYP_SCOPE;

    attributes.SetDrawableLayer(drawable_layer);

    GraphicsPipelineRef graphics_pipeline;

    auto it = m_graphics_pipelines.Find(attributes);

    if (it != m_graphics_pipelines.End())
    {
        graphics_pipeline = it->second.Lock();
    }

    if (!graphics_pipeline)
    {
        graphics_pipeline = g_engine->GetGraphicsPipelineCache()->GetOrCreate(
            m_shader,
            m_descriptor_table,
            {},
            attributes);

        /// FIXME: GetCurrentView() should not be used here
        // const FramebufferRef& framebuffer = g_engine->GetCurrentView()->GetGBuffer()->GetBucket(attributes.GetMaterialAttributes().bucket).GetFramebuffer();
        // render_group->AddFramebuffer(framebuffer);

        if (it != m_graphics_pipelines.End())
        {
            it->second = graphics_pipeline;
        }
        else
        {
            m_graphics_pipelines.Insert(attributes, graphics_pipeline);
        }
    }

    return graphics_pipeline;
}

#pragma endregion DebugDrawer

#pragma region DebugDrawCommandList

void DebugDrawCommandList::Push(UniquePtr<DebugDrawCommand>&& command)
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
