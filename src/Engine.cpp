#include <Engine.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>
#include <rendering/backend/AsyncCompute.hpp>

#include <scene/controllers/FollowCameraController.hpp>

#include <Game.hpp>
#include <GameThread.hpp>

#include <util/MeshBuilder.hpp>

#include <audio/AudioManager.hpp>

namespace hyperion::v2 {

using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;
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
    ImageRef image;
    GPUBufferRef buffer;

    RENDER_COMMAND(CopyBackbufferToCPU)(const ImageRef &image, const GPUBufferRef &buffer)
        : image(image),
          buffer(buffer)
    {
    }

    virtual Result operator()()
    {
        AssertThrow(image.IsValid());
        AssertThrow(buffer.IsValid());


        HYPERION_RETURN_OK;
    }
};

#pragma endregion


Engine::Engine()
    : m_placeholder_data(new PlaceholderData()),
      m_global_descriptor_table(MakeRenderObject<renderer::DescriptorTable>(*renderer::g_static_descriptor_table_decl))
{
}

Engine::~Engine()
{
    AssertThrowMsg(m_instance == nullptr, "Engine instance must be destroyed before Engine object is destroyed");
}

bool Engine::InitializeGame(Game *game)
{
    AssertThrow(game != nullptr);
    AssertThrowMsg(game_thread == nullptr || !game_thread->IsRunning(), "Game thread already running; cannot initialize game instance");

    Threads::AssertOnThread(THREAD_MAIN, "Must be on main thread to initialize game instance");

    game->Init();

    if (game_thread == nullptr) {
        game_thread.Reset(new GameThread);
    }

    return game_thread->Start(game);
}

void Engine::FindTextureFormatDefaults()
{
    Threads::AssertOnThread(THREAD_RENDER);

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
            std::array{ //InternalFormat::RG16,
                        InternalFormat::R11G11B10F,
                        InternalFormat::RGBA16F,
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


void Engine::Initialize(RC<Application> application)
{
    m_application = application;

    Threads::AssertOnThread(THREAD_MAIN);
    Threads::SetCurrentThreadPriority(ThreadPriorityValue::HIGHEST);

    game_thread.Reset(new GameThread);

    m_crash_handler.Initialize();

    TaskSystem::GetInstance().Start();

    AssertThrow(m_instance == nullptr);
    m_instance.Reset(new Instance(application));
    HYPERION_ASSERT_RESULT(m_instance->Initialize(use_debug_layers));

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
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(ScenesBuffer), m_render_data->scenes.GetBuffer());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(LightsBuffer), m_render_data->lights.GetBuffer());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(ObjectsBuffer), m_render_data->objects.GetBuffer());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(CamerasBuffer), m_render_data->cameras.GetBuffer());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(EnvGridsBuffer), m_render_data->env_grids.GetBuffer());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(EnvProbesBuffer), m_render_data->env_probes.GetBuffer());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(CurrentEnvProbe), m_render_data->env_probes.GetBuffer());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Scene), frame_index)->SetElement(HYP_NAME(ShadowMapsBuffer), m_render_data->shadow_map_data.GetBuffer());
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
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Object), frame_index)->SetElement(HYP_NAME(MaterialsBuffer), m_render_data->materials.GetBuffer());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Object), frame_index)->SetElement(HYP_NAME(SkeletonsBuffer), m_render_data->skeletons.GetBuffer());
        m_global_descriptor_table->GetDescriptorSet(HYP_NAME(Object), frame_index)->SetElement(HYP_NAME(EntityInstanceBatchesBuffer), m_render_data->entity_instance_batches.GetBuffer());

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

    m_render_list_container.Create();

    // has to be after we create framebuffers
    m_debug_drawer.Create();

    AssertThrowMsg(AudioManager::GetInstance()->Initialize(), "Failed to initialize audio device");

    m_final_pass.Create();

    m_render_list_container.AddFramebuffersToRenderGroups();

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

    m_deferred_renderer.Create();
    
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
    Threads::AssertOnThread(THREAD_MAIN);

    m_is_stopping = true;
    m_is_render_loop_active = false;

    DebugLog(
        LogType::Debug,
        "Stopping all engine processes\n"
    );

    // Force execute any remaining render commands
    // HYP_SYNC_RENDER();

    // Wait for any remaining frames to finish
    // HYPERION_ASSERT_RESULT(GetGPUInstance()->GetDevice()->Wait());

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

    // Stop game thread and wait for it to finish
    if (game_thread != nullptr) {
        DebugLog(
            LogType::Debug,
            "Stopping game thread\n"
        );

        game_thread->Stop();
        while (game_thread->IsRunning()) {
            // DebugLog(
            //     LogType::Debug,
            //     "Waiting for game thread to stop\n"
            // );

            Threads::Sleep(1);
        }
        game_thread->Join();
    }

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Destroy(m_instance->GetDevice()));

    m_render_list_container.Destroy();
    m_deferred_renderer.Destroy();

    m_final_pass.Destroy();

    // delete placeholder data
    m_placeholder_data->Destroy();

    // delete debug drawer mode
    m_debug_drawer.Destroy();

    m_render_data->Destroy();

    { // here we delete all the objects that will be enqueued to be deleted
        // delete objects that are enqueued for deletion
        g_safe_deleter->ForceReleaseAll();

        // delete all render objects
        // TEMP Hack: deletion of render objects may cause other render objects to be enqueued
        RenderObjectDeleter<renderer::Platform::CURRENT>::ForceDeleteAll();
        RenderObjectDeleter<renderer::Platform::CURRENT>::ForceDeleteAll();
        RenderObjectDeleter<renderer::Platform::CURRENT>::ForceDeleteAll();
        RenderObjectDeleter<renderer::Platform::CURRENT>::ForceDeleteAll();
    }

    m_render_group_mapping.Clear();

    AssertThrow(m_instance != nullptr);
    m_instance->Destroy();
}

void Engine::RenderNextFrame(Game *game)
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

    m_final_pass.Render(frame);

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

    Handle<Shader> shader = g_shader_manager->GetOrCreate(shader_definition);

    if (!shader) {
        DebugLog(
            LogType::Error,
            "Shader is empty; Cannot create RenderGroup.\n"
        );

        return Handle<RenderGroup>::empty;
    }

    // create a RenderGroup with the given params
    auto renderer_instance = CreateObject<RenderGroup>(
        std::move(shader),
        renderable_attributes
    );

    DebugLog(
        LogType::Debug,
        "Created RenderGroup for RenderableAttributeSet with hash %llu from thread %s\n",
        renderable_attributes.GetHashCode().Value(),
        Threads::CurrentThreadID().name.LookupString()
    );

    std::lock_guard guard(m_render_group_mapping_mutex);

    AddRenderGroupInternal(renderer_instance, false);

    return renderer_instance;
}

// Handle<RenderGroup> Engine::CreateRenderGroup(
//     const Handle<Shader> &shader,
//     const RenderableAttributeSet &renderable_attributes
// )
// {

//     if (!shader) {
//         DebugLog(
//             LogType::Error,
//             "Shader is empty; Cannot create RenderGroup.\n"
//         );

//         return Handle<RenderGroup>::empty;
//     }


// }

Handle<RenderGroup> Engine::CreateRenderGroup(
    const Handle<Shader> &shader,
    const RenderableAttributeSet &renderable_attributes,
    DescriptorTableRef descriptor_table
)
{
    if (!shader) {
        DebugLog(
            LogType::Error,
            "Shader is empty; Cannot create RenderGroup.\n"
        );

        return Handle<RenderGroup>::empty;
    }

    RenderableAttributeSet new_renderable_attributes(renderable_attributes);
    new_renderable_attributes.SetShaderDefinition(shader->GetCompiledShader().GetDefinition());

    auto &render_list_bucket = m_render_list_container.Get(new_renderable_attributes.GetMaterialAttributes().bucket);

    // create a RenderGroup with the given params
    auto renderer_instance = CreateObject<RenderGroup>(
        Handle<Shader>(shader),
        new_renderable_attributes,
        std::move(descriptor_table)
    );

    return renderer_instance;
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

    m_render_list_container
        .Get(render_group->GetRenderableAttributes().GetMaterialAttributes().bucket)
        .AddRenderGroup(render_group);
}

void Engine::PreFrameUpdate(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_render_list_container.AddPendingRenderGroups();

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

    m_deferred_renderer.GetPostProcessing().PerformUpdates();

    m_material_descriptor_set_manager.Update(frame);

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Update(m_instance->GetDevice(), frame_index));

    RenderObjectDeleter<renderer::Platform::CURRENT>::Iterate();

    g_safe_deleter->PerformEnqueuedDeletions();
}

void Engine::RenderDeferred(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_deferred_renderer.Render(frame, render_state.GetScene().render_environment);
}

// GlobalDescriptorSetManager
GlobalDescriptorSetManager::GlobalDescriptorSetManager(Engine *engine)
{
    Mutex::Guard guard(m_mutex);

    for (auto &it : renderer::g_static_descriptor_table_decl->GetElements()) {
        renderer::DescriptorSetLayout layout(it);

        DescriptorSet2Ref ref = layout.CreateDescriptorSet();
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
    Threads::AssertOnThread(THREAD_RENDER);

    Mutex::Guard guard(m_mutex);

    for (auto &it : m_descriptor_sets) {
        HYPERION_ASSERT_RESULT(it.second->Create(engine->GetGPUDevice()));
    }
}

void GlobalDescriptorSetManager::AddDescriptorSet(Name name, const DescriptorSet2Ref &ref)
{
    Mutex::Guard guard(m_mutex);

    const auto insert_result = m_descriptor_sets.Insert(name, std::move(ref));
    AssertThrowMsg(insert_result.second, "Failed to insert descriptor set, item %s already exists", name.LookupString());
}

DescriptorSet2Ref GlobalDescriptorSetManager::GetDescriptorSet(Name name) const
{
    Mutex::Guard guard(m_mutex);

    auto it = m_descriptor_sets.Find(name);

    if (it != m_descriptor_sets.End()) {
        return it->second;
    }

    return DescriptorSet2Ref { };
}

} // namespace hyperion::v2
