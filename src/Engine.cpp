/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/AsyncCompute.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

#include <Game.hpp>
#include <core/threading/GameThread.hpp>

#include <util/MeshBuilder.hpp>
#include <util/fs/FsUtil.hpp>

#include <audio/AudioManager.hpp>

#include <core/system/StackDump.hpp>

namespace hyperion {

using renderer::Attachment;
using renderer::ImageView;
using renderer::Framebuffer;
using renderer::FillMode;
using renderer::GPUBufferType;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;

Engine              *g_engine = nullptr;
AssetManager        *g_asset_manager = nullptr;
ShaderManagerSystem *g_shader_manager = nullptr;
MaterialCache       *g_material_system = nullptr;
SafeDeleter         *g_safe_deleter = nullptr;

#pragma region Render commands

struct RENDER_COMMAND(CopyBackbufferToCPU) : renderer::RenderCommand
{
    ImageRef        image;
    GPUBufferRef    buffer;

    RENDER_COMMAND(CopyBackbufferToCPU)(const ImageRef &image, const GPUBufferRef &buffer)
        : image(image),
          buffer(buffer)
    {
    }

    virtual ~RENDER_COMMAND(CopyBackbufferToCPU)() override = default;

    virtual Result operator()() override
    {
        AssertThrow(image.IsValid());
        AssertThrow(buffer.IsValid());


        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

Engine *Engine::GetInstance()
{
    return g_engine;
}

Engine::Engine()
    : m_placeholder_data(new PlaceholderData()),
      m_global_descriptor_table(MakeRenderObject<renderer::DescriptorTable>(*renderer::g_static_descriptor_table_decl))
{
}

Engine::~Engine()
{
    AssertThrowMsg(m_instance == nullptr, "Engine instance must be destroyed before Engine object is destroyed");
}

HYP_API bool Engine::InitializeGame(Game *game)
{
    AssertThrow(game != nullptr);

    Threads::AssertOnThread(ThreadName::THREAD_MAIN, "Must be on main thread to initialize game instance");

    game->Init_Internal();

    return true;
}

void Engine::FindTextureFormatDefaults()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const Device *device = m_instance->GetDevice();

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_COLOR,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::R10G10B10A2,
                        InternalFormat::RGBA16F,
                        InternalFormat::RGBA8 },
            renderer::ImageSupportType::SRV
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::DEPTH_32F, InternalFormat::DEPTH_24,
                        InternalFormat::DEPTH_16
                         },
            renderer::ImageSupportType::DEPTH
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_NORMALS,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::RGBA16F,
                        InternalFormat::RGBA32F,
                        InternalFormat::RGBA8 },
            renderer::ImageSupportType::SRV
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_STORAGE,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::RGBA16F },
            renderer::ImageSupportType::UAV
        )
    );
}

HYP_API void Engine::Initialize(const RC<AppContext> &app_context)
{
    AssertThrow(app_context != nullptr);

    m_app_context = app_context;

    Threads::AssertOnThread(ThreadName::THREAD_MAIN);
    Threads::SetCurrentThreadPriority(ThreadPriorityValue::HIGHEST);
    
    RenderObjectDeleter<renderer::Platform::CURRENT>::Initialize();

    m_crash_handler.Initialize();

    TaskSystem::GetInstance().Start();

    AssertThrow(m_instance == nullptr);
    m_instance.Reset(new Instance());

    HYPERION_ASSERT_RESULT(m_instance->Initialize(*app_context, use_debug_layers));

    FindTextureFormatDefaults();

    m_configuration.SetToDefaultConfiguration();
    m_configuration.LoadFromDefinitionsFile();

    // save default configuration to file if
    // anything changed from the loading process
    if (!m_configuration.SaveToDefinitionsFile()) {
        DebugLog(
            LogType::Error,
            "Failed to save configuration file\n"
        );
    }

    if (!m_shader_compiler.LoadShaderDefinitions()) {
        HYP_BREAKPOINT;
    }

    m_render_data.Reset(new ShaderGlobals());
    m_render_data->Create();

    m_placeholder_data->Create();

    m_world = CreateObject<World>();
    InitObject(m_world);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        // Global

        for (uint i = 0; i < num_gbuffer_textures; i++) {
            m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(GBufferTextures), i, GetPlaceholderData()->GetImageView2D1x1R8());
        }

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(GBufferDepthTexture), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(GBufferMipChain), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(BlueNoiseBuffer), GetPlaceholderData()->GetOrCreateBuffer(GetGPUDevice(), GPUBufferType::STORAGE_BUFFER, sizeof(BlueNoiseBuffer), true /* exact size */));

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(DeferredResult), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(PostFXPreStack), 0, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(PostFXPreStack), 1, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(PostFXPreStack), 2, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(PostFXPreStack), 3, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(PostFXPostStack), 0, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(PostFXPostStack), 1, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(PostFXPostStack), 2, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(PostFXPostStack), 3, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(PostProcessingUniforms), GetPlaceholderData()->GetOrCreateBuffer(GetGPUDevice(), GPUBufferType::CONSTANT_BUFFER, sizeof(PostProcessingUniforms), true /* exact size */));

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(SSAOResultTexture), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(SSRResultTexture), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(TAAResultTexture), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(RTRadianceResultTexture), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(EnvGridIrradianceResultTexture), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(EnvGridRadianceResultTexture), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(ReflectionProbeResultTexture), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(DeferredIndirectResultTexture), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(DeferredDirectResultTexture), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(DepthPyramidResult), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(DDGIUniforms), GetPlaceholderData()->GetOrCreateBuffer(GetGPUDevice(), GPUBufferType::CONSTANT_BUFFER, sizeof(DDGIUniforms), true /* exact size */));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(DDGIIrradianceTexture), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(DDGIDepthTexture), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(SamplerNearest), GetPlaceholderData()->GetSamplerNearest());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(SamplerLinear), GetPlaceholderData()->GetSamplerLinear());

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(UITexture), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Global), frame_index)->SetElement(HYP_NAME(FinalOutputTexture), GetPlaceholderData()->GetImageView2D1x1R8());

        // Scene
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(ScenesBuffer), m_render_data->scenes.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(LightsBuffer), m_render_data->lights.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(ObjectsBuffer), m_render_data->objects.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(CamerasBuffer), m_render_data->cameras.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(EnvGridsBuffer), m_render_data->env_grids.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(EnvProbesBuffer), m_render_data->env_probes.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(CurrentEnvProbe), m_render_data->env_probes.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(ShadowMapsBuffer), m_render_data->shadow_map_data.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(SHGridBuffer), GetRenderData()->spherical_harmonics_grid.sh_grid_buffer);

        for (uint shadow_map_index = 0; shadow_map_index < max_shadow_maps; shadow_map_index++) {
            m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(ShadowMapTextures), shadow_map_index, GetPlaceholderData()->GetImageView2D1x1R8());
        }

        for (uint shadow_map_index = 0; shadow_map_index < max_bound_point_shadow_maps; shadow_map_index++) {
            m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(PointLightShadowMapTextures), shadow_map_index, GetPlaceholderData()->GetImageViewCube1x1R8());
        }

        for (uint i = 0; i < max_bound_reflection_probes; i++) {
            m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(EnvProbeTextures), i, GetPlaceholderData()->GetImageViewCube1x1R8());
        }

        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(VoxelGridTexture), GetPlaceholderData()->GetImageView3D1x1x1R8());

        // Object
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Object), frame_index)->SetElement(HYP_NAME(MaterialsBuffer), m_render_data->materials.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Object), frame_index)->SetElement(HYP_NAME(SkeletonsBuffer), m_render_data->skeletons.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Object), frame_index)->SetElement(HYP_NAME(EntityInstanceBatchesBuffer), m_render_data->entity_instance_batches.GetBuffer(frame_index));

        // Material
#ifdef HYP_FEATURES_BINDLESS_TEXTURES
        for (uint texture_index = 0; texture_index < max_bindless_resources; texture_index++) {
            m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Material), frame_index)
                ->SetElement(HYP_NAME(Textures), texture_index, GetPlaceholderData()->GetImageView2D1x1R8());
        }
#else
        for (uint texture_index = 0; texture_index < max_bound_textures; texture_index++) {
            m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Material), frame_index)
                ->SetElement(HYP_NAME(Textures), texture_index, GetPlaceholderData()->GetImageView2D1x1R8());
        }
#endif
    }

    // m_global_descriptor_set_manager.Initialize(this);

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Create(m_instance->GetDevice()));

    m_material_descriptor_set_manager.Initialize();

    m_gbuffer.Create();

    // has to be after we create framebuffers
    m_debug_drawer.Create();

    AssertThrowMsg(AudioManager::GetInstance()->Initialize(), "Failed to initialize audio device");

    m_final_pass.Reset(new FinalPass);
    m_final_pass->Create();

    Compile();
}

void Engine::Compile()
{
    for (uint i = 0; i < max_frames_in_flight; i++) {
        /* Finalize env probes */
        m_render_data->env_probes.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize env grids */
        m_render_data->env_grids.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize shadow maps */
        m_render_data->shadow_map_data.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize lights */
        m_render_data->lights.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize skeletons */
        m_render_data->skeletons.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize materials */
        m_render_data->materials.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize per-object data */
        m_render_data->objects.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize scene data */
        m_render_data->scenes.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize camera data */
        m_render_data->cameras.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize debug draw data */
        m_render_data->immediate_draws.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize instance batch data */
        m_render_data->entity_instance_batches.UpdateBuffer(m_instance->GetDevice(), i);
    }

    m_deferred_renderer.Reset(new DeferredRenderer);
    m_deferred_renderer->Create();
    
    HYP_SYNC_RENDER();

    m_is_render_loop_active = true;
}

void Engine::RequestStop()
{
    m_is_render_loop_active = false;
    //m_stop_requested.Set(true, MemoryOrder::RELAXED);
}

void Engine::FinalizeStop()
{
    Threads::AssertOnThread(ThreadName::THREAD_MAIN);

    m_is_shutting_down.Set(true, MemoryOrder::SEQUENTIAL);
    m_is_render_loop_active = false;

    DebugLog(
        LogType::Debug,
        "Stopping all engine processes\n"
    );

    m_delegates.OnShutdown.Broadcast();

    m_world.Reset();

    if (TaskSystem::GetInstance().IsRunning()) { // Stop task system
        DebugLog(
            LogType::Debug,
            "Stopping task system\n"
        );

        TaskSystem::GetInstance().Stop();

        DebugLog(
            LogType::Debug,
            "Task system stopped\n"
        );
    }

    m_gbuffer.Destroy();

    m_deferred_renderer->Destroy();
    m_deferred_renderer.Reset();

    m_final_pass->Destroy();
    m_final_pass.Reset();

    m_debug_drawer.Destroy();

    m_render_data->Destroy();

    m_render_group_mapping.Clear();

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Destroy(m_instance->GetDevice()));

    m_placeholder_data->Destroy();

    HYPERION_ASSERT_RESULT(m_instance->GetDevice()->Wait());

    g_safe_deleter->ForceDeleteAll();
    ForceDeleteAllEnqueuedRenderObjects<renderer::Platform::CURRENT>();

    HYPERION_ASSERT_RESULT(m_instance->GetDevice()->Wait());

    HYPERION_ASSERT_RESULT(m_instance->Destroy());
}

HYP_API void Engine::RenderNextFrame(Game *game)
{
    // if (m_stop_requested.Get(MemoryOrder::RELAXED)) {
    //     FinalizeStop();

    //     return;
    // }

    Result frame_result = GetGPUInstance()->GetFrameHandler()->PrepareFrame(
        GetGPUInstance()->GetDevice(),
        GetGPUInstance()->GetSwapchain()
    );

    if (!frame_result) {
        m_crash_handler.HandleGPUCrash(frame_result);

        m_is_render_loop_active = false;
        RequestStop();
    }

    const FrameRef &frame = GetGPUInstance()->GetFrameHandler()->GetCurrentFrame();
    
    HYPERION_ASSERT_RESULT(GetGPUDevice()->GetAsyncCompute()->PrepareForFrame(GetGPUDevice(), frame));

    PreFrameUpdate(frame);

    HYPERION_ASSERT_RESULT(frame->BeginCapture(GetGPUInstance()->GetDevice()));
    
    m_world->PreRender(frame);

    game->OnFrameBegin(frame);

    m_world->Render(frame);

    RenderDeferred(frame);

    m_final_pass->Render(frame);

    HYPERION_ASSERT_RESULT(frame->EndCapture(GetGPUInstance()->GetDevice()));

    frame_result = frame->Submit(&GetGPUDevice()->GetGraphicsQueue());

    if (!frame_result) {
        m_crash_handler.HandleGPUCrash(frame_result);

        m_is_render_loop_active = false;
        RequestStop();
    }

    game->OnFrameEnd(frame);

    HYPERION_ASSERT_RESULT(GetGPUDevice()->GetAsyncCompute()->Submit(GetGPUDevice(), frame));

    GetGPUInstance()->GetFrameHandler()->PresentFrame(&GetGPUDevice()->GetGraphicsQueue(), GetGPUInstance()->GetSwapchain());
    GetGPUInstance()->GetFrameHandler()->NextFrame();
}

Handle<RenderGroup> Engine::CreateRenderGroup(const RenderableAttributeSet &renderable_attributes)
{
    const ShaderDefinition &shader_definition = renderable_attributes.GetShaderDefinition();
    AssertThrowMsg(shader_definition, "Shader definition is unset");

    ShaderRef shader = g_shader_manager->GetOrCreate(shader_definition);

    if (!shader) {
        DebugLog(
            LogType::Error,
            "Shader is empty; Cannot create RenderGroup.\n"
        );

        return Handle<RenderGroup>::empty;
    }

    // create a RenderGroup with the given params
    Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
        shader,
        renderable_attributes
    );

    DebugLog(
        LogType::Debug,
        "Created RenderGroup for RenderableAttributeSet with hash %llu from thread %s\n",
        renderable_attributes.GetHashCode().Value(),
        Threads::CurrentThreadID().name.LookupString()
    );

    std::lock_guard guard(m_render_group_mapping_mutex);

    AddRenderGroupInternal(render_group, false);

    return render_group;
}

Handle<RenderGroup> Engine::CreateRenderGroup(
    const ShaderRef &shader,
    const RenderableAttributeSet &renderable_attributes,
    const DescriptorTableRef &descriptor_table
)
{
    if (!shader.IsValid()) {
        DebugLog(
            LogType::Error,
            "Shader is empty; Cannot create RenderGroup.\n"
        );

        return Handle<RenderGroup>::empty;
    }

    if (!shader->GetCompiledShader()) {
        DebugLog(
            LogType::Error,
            "Shader is not compiled; Cannot create RenderGroup.\n"
        );

        return Handle<RenderGroup>::empty;
    }

    RenderableAttributeSet new_renderable_attributes(renderable_attributes);
    new_renderable_attributes.SetShaderDefinition(shader->GetCompiledShader()->GetDefinition());
    
    // create a RenderGroup with the given params
    Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
        shader,
        new_renderable_attributes,
        descriptor_table
    );

    return render_group;
}

void Engine::AddRenderGroup(Handle<RenderGroup> &render_group)
{
    std::lock_guard guard(m_render_group_mapping_mutex);

    AddRenderGroupInternal(render_group, true);
}

void Engine::AddRenderGroupInternal(Handle<RenderGroup> &render_group, bool cache)
{
    if (cache) {
        DebugLog(
            LogType::Debug,
            "Insert RenderGroup in mapping for renderable attribute set hash %llu\n",
            render_group->GetRenderableAttributes().GetHashCode().Value()
        );

        m_render_group_mapping.Insert(
            render_group->GetRenderableAttributes(),
            render_group
        );
    }

    m_gbuffer
        .Get(render_group->GetRenderableAttributes().GetMaterialAttributes().bucket)
        .AddRenderGroup(render_group);
}

void Engine::PreFrameUpdate(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    HYPERION_ASSERT_RESULT(renderer::RenderCommands::Flush());

    UpdateBuffersAndDescriptors(frame);

    ResetRenderState(RENDER_STATE_ACTIVE_ENV_PROBE | RENDER_STATE_SCENE | RENDER_STATE_CAMERA);
}

void Engine::ResetRenderState(RenderStateMask mask)
{
    render_state.Reset(mask);
}

void Engine::UpdateBuffersAndDescriptors(Frame *frame)
{
    const uint frame_index = frame->GetFrameIndex();

    m_render_data->scenes.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_render_data->cameras.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_render_data->objects.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_render_data->materials.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_render_data->skeletons.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_render_data->lights.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_render_data->shadow_map_data.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_render_data->env_probes.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_render_data->env_grids.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_render_data->immediate_draws.UpdateBuffer(m_instance->GetDevice(), frame_index);
    m_render_data->entity_instance_batches.UpdateBuffer(m_instance->GetDevice(), frame_index);

    m_deferred_renderer->GetPostProcessing().PerformUpdates();

    m_material_descriptor_set_manager.Update(frame);

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Update(m_instance->GetDevice(), frame_index));

    RenderObjectDeleter<renderer::Platform::CURRENT>::Iterate();

    g_safe_deleter->PerformEnqueuedDeletions();
}

void Engine::RenderDeferred(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    m_deferred_renderer->Render(frame, render_state.GetScene().render_environment);
}

// GlobalDescriptorSetManager
GlobalDescriptorSetManager::GlobalDescriptorSetManager(Engine *engine)
{
    Mutex::Guard guard(m_mutex);

    for (auto &it : renderer::g_static_descriptor_table_decl->GetElements()) {
        renderer::DescriptorSetLayout layout(it);

        DescriptorSetRef ref = layout.CreateDescriptorSet();
        AssertThrow(ref.IsValid());

        DebugLog(LogType::Debug, "Num elements for descriptor set %s: %u\n", ref.GetName().LookupString(), ref->GetLayout().GetElements().Size());
        HYP_BREAKPOINT;

        // Init with placeholder data
        for (const auto &layout_it : ref->GetLayout().GetElements()) {
            switch (layout_it.second.type) {
            case renderer::DescriptorSetElementType::UNIFORM_BUFFER: // Fallthrough
            case renderer::DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC: {
                AssertThrowMsg(layout_it.second.size != uint(-1), "No size set for descriptor %s", layout_it.first.LookupString());

                for (uint i = 0; i < layout_it.second.count; i++) {
                    ref->SetElement(
                        layout_it.first,
                        i,
                        engine->GetPlaceholderData()->GetOrCreateBuffer(
                            engine->GetGPUDevice(),
                            renderer::GPUBufferType::CONSTANT_BUFFER,
                            layout_it.second.size,
                            true // exact_size
                        )
                    );
                }

                break;
            }
            case renderer::DescriptorSetElementType::STORAGE_BUFFER: // Fallthrough
            case renderer::DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC: {
                AssertThrowMsg(layout_it.second.size != uint(-1), "No size set for descriptor %s", layout_it.first.LookupString());

                for (uint i = 0; i < layout_it.second.count; i++) {
                    ref->SetElement(
                        layout_it.first,
                        i,
                        engine->GetPlaceholderData()->GetOrCreateBuffer(
                            engine->GetGPUDevice(),
                            renderer::GPUBufferType::STORAGE_BUFFER,
                            layout_it.second.size,
                            true // exact_size
                        )
                    );
                }

                break;
            }
            case renderer::DescriptorSetElementType::IMAGE: {
                // @TODO: Differentiate between 2D, 3D, and Cubemap
                for (uint i = 0; i < layout_it.second.count; i++) {
                    ref->SetElement(
                        layout_it.first,
                        i,
                        engine->GetPlaceholderData()->GetImageView2D1x1R8()
                    );
                }

                break;
            }
            case renderer::DescriptorSetElementType::IMAGE_STORAGE: {
                // @TODO: Differentiate between 2D, 3D, and Cubemap
                for (uint i = 0; i < layout_it.second.count; i++) {
                    ref->SetElement(
                        layout_it.first,
                        i,
                        engine->GetPlaceholderData()->GetImageView2D1x1R8Storage()
                    );
                }

                break;
            }
            case renderer::DescriptorSetElementType::SAMPLER: {
                for (uint i = 0; i < layout_it.second.count; i++) {
                    ref->SetElement(
                        layout_it.first,
                        i,
                        engine->GetPlaceholderData()->GetSamplerNearest()
                    );
                }

                break;
            }
            case renderer::DescriptorSetElementType::TLAS: {
                // Do nothing, must be manually set.
                break;
            }
            }
        }

        m_descriptor_sets.Insert(it.name, std::move(ref));
    }
}

GlobalDescriptorSetManager::~GlobalDescriptorSetManager() = default;

void GlobalDescriptorSetManager::Initialize(Engine *engine)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    Mutex::Guard guard(m_mutex);

    for (auto &it : m_descriptor_sets) {
        HYPERION_ASSERT_RESULT(it.second->Create(engine->GetGPUDevice()));
    }
}

void GlobalDescriptorSetManager::AddDescriptorSet(Name name, const DescriptorSetRef &ref)
{
    Mutex::Guard guard(m_mutex);

    const auto insert_result = m_descriptor_sets.Insert(name, std::move(ref));
    AssertThrowMsg(insert_result.second, "Failed to insert descriptor set, item %s already exists", name.LookupString());
}

DescriptorSetRef GlobalDescriptorSetManager::GetDescriptorSet(Name name) const
{
    Mutex::Guard guard(m_mutex);

    auto it = m_descriptor_sets.Find(name);

    if (it != m_descriptor_sets.End()) {
        return it->second;
    }

    return DescriptorSetRef { };
}

} // namespace hyperion
