/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/GBuffer.hpp>

#include <rendering/backend/AsyncCompute.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <Game.hpp>

#include <util/profiling/ProfileScope.hpp>
#include <util/MeshBuilder.hpp>
#include <util/fs/FsUtil.hpp>

#include <audio/AudioManager.hpp>

#include <core/system/StackDump.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/net/NetRequestThread.hpp>

#include <scripting/ScriptingService.hpp>

namespace hyperion {

using renderer::FillMode;
using renderer::GPUBufferType;

Engine              *g_engine = nullptr;
AssetManager        *g_asset_manager = nullptr;
ShaderManagerSystem *g_shader_manager = nullptr;
MaterialCache       *g_material_system = nullptr;
SafeDeleter         *g_safe_deleter = nullptr;

/* \brief Should the swapchain be rebuilt on the next frame? */
static bool g_should_recreate_swapchain = false;

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

struct RENDER_COMMAND(RecreateSwapchain) : renderer::RenderCommand
{
    virtual ~RENDER_COMMAND(RecreateSwapchain)() override = default;

    virtual Result operator()() override
    {
        g_should_recreate_swapchain = true;

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
      m_global_descriptor_table(MakeRenderObject<DescriptorTable>(*renderer::g_static_descriptor_table_decl)),
      m_is_initialized(false)
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
            std::array{ InternalFormat::RGBA8_SRGB,
                        InternalFormat::R10G10B10A2,
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
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertThrow(!m_is_initialized);

    Threads::SetCurrentThreadPriority(ThreadPriorityValue::HIGHEST);

    AssertThrow(app_context != nullptr);
    m_app_context = app_context;

    m_app_context->GetMainWindow()->OnWindowSizeChanged.Bind([this](Vec2u new_window_size)
    {
        HYP_LOG(Engine, LogLevel::INFO, "Resize window to {}", new_window_size);

        PUSH_RENDER_COMMAND(RecreateSwapchain);
    }).Detach();
    
    RenderObjectDeleter<renderer::Platform::CURRENT>::Initialize();

    m_crash_handler.Initialize();

    TaskSystem::GetInstance().Start();

    AssertThrow(m_instance == nullptr);
    m_instance.Reset(new Instance());

    HYPERION_ASSERT_RESULT(m_instance->Initialize(*app_context, use_debug_layers));

    // Update app configuration to reflect device, after instance is created (e.g RT is not supported)
    m_app_context->UpdateConfigurationOverrides();

    FindTextureFormatDefaults();

    m_configuration.SetToDefaultConfiguration();
    m_configuration.LoadFromDefinitionsFile();

    if (!m_shader_compiler.LoadShaderDefinitions()) {
        HYP_BREAKPOINT;
    }

    m_render_data.Reset(new ShaderGlobals());
    m_render_data->Create();

    m_placeholder_data->Create();

    // Create script compilation service
    m_scripting_service.Reset(new ScriptingService(
        g_asset_manager->GetBasePath() / "scripts" / "src",
        g_asset_manager->GetBasePath() / "scripts" / "projects",
        g_asset_manager->GetBasePath() / "scripts" / "bin"
    ));

    m_scripting_service->Start();

    m_net_request_thread.Reset(new net::NetRequestThread);
    m_net_request_thread->Start();

    // must start after net request thread
    StartProfilerConnectionThread();

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        // Global

        for (uint i = 0; i < num_gbuffer_textures; i++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("GBufferTextures"), i, GetPlaceholderData()->GetImageView2D1x1R8());
        }

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("GBufferDepthTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("GBufferMipChain"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("BlueNoiseBuffer"), GetPlaceholderData()->GetOrCreateBuffer(GetGPUDevice(), GPUBufferType::STORAGE_BUFFER, sizeof(BlueNoiseBuffer), true /* exact size */));

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DeferredResult"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPreStack"), 0, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPreStack"), 1, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPreStack"), 2, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPreStack"), 3, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPostStack"), 0, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPostStack"), 1, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPostStack"), 2, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPostStack"), 3, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostProcessingUniforms"), GetPlaceholderData()->GetOrCreateBuffer(GetGPUDevice(), GPUBufferType::CONSTANT_BUFFER, sizeof(PostProcessingUniforms), true /* exact size */));

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SSAOResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SSRResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("TAAResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("RTRadianceResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvGridIrradianceResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvGridRadianceResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ReflectionProbeResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DeferredIndirectResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DeferredDirectResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DepthPyramidResult"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIUniforms"), GetPlaceholderData()->GetOrCreateBuffer(GetGPUDevice(), GPUBufferType::CONSTANT_BUFFER, sizeof(DDGIUniforms), true /* exact size */));
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIIrradianceTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIDepthTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SamplerNearest"), GetPlaceholderData()->GetSamplerNearest());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SamplerLinear"), GetPlaceholderData()->GetSamplerLinearMipmap());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("UITexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("FinalOutputTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        // Scene
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("ScenesBuffer"), m_render_data->scenes.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("LightsBuffer"), m_render_data->lights.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("ObjectsBuffer"), m_render_data->objects.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("CamerasBuffer"), m_render_data->cameras.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("EnvGridsBuffer"), m_render_data->env_grids.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("EnvProbesBuffer"), m_render_data->env_probes.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("CurrentEnvProbe"), m_render_data->env_probes.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("ShadowMapsBuffer"), m_render_data->shadow_map_data.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("SHGridBuffer"), GetRenderData()->spherical_harmonics_grid.sh_grid_buffer);

        for (uint shadow_map_index = 0; shadow_map_index < max_shadow_maps; shadow_map_index++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("ShadowMapTextures"), shadow_map_index, GetPlaceholderData()->GetImageView2D1x1R8());
        }

        for (uint shadow_map_index = 0; shadow_map_index < max_bound_point_shadow_maps; shadow_map_index++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("PointLightShadowMapTextures"), shadow_map_index, GetPlaceholderData()->GetImageViewCube1x1R8());
        }

        for (uint i = 0; i < max_bound_reflection_probes; i++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("EnvProbeTextures"), i, GetPlaceholderData()->GetImageViewCube1x1R8());
        }

        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("VoxelGridTexture"), GetPlaceholderData()->GetImageView3D1x1x1R8());

        // Object
        m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("MaterialsBuffer"), m_render_data->materials.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("SkeletonsBuffer"), m_render_data->skeletons.GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("EntityInstanceBatchesBuffer"), m_render_data->entity_instance_batches.GetBuffer(frame_index));

        // Material
#ifdef HYP_FEATURES_BINDLESS_TEXTURES
        for (uint texture_index = 0; texture_index < max_bindless_resources; texture_index++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Material"), frame_index)
                ->SetElement(NAME("Textures"), texture_index, GetPlaceholderData()->GetImageView2D1x1R8());
        }
#else
        for (uint texture_index = 0; texture_index < max_bound_textures; texture_index++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Material"), frame_index)
                ->SetElement(NAME("Textures"), texture_index, GetPlaceholderData()->GetImageView2D1x1R8());
        }
#endif
    }

    // m_global_descriptor_set_manager.Initialize(this);

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Create(m_instance->GetDevice()));

    m_material_descriptor_set_manager.Initialize();

    AssertThrowMsg(AudioManager::GetInstance()->Initialize(), "Failed to initialize audio device");

    m_deferred_renderer.Reset(new DeferredRenderer);
    m_deferred_renderer->Create();

    m_final_pass.Reset(new FinalPass);
    m_final_pass->Create();

    m_debug_drawer.Create();

    m_world = CreateObject<World>();
    InitObject(m_world);
    
    HYP_SYNC_RENDER();

    m_is_render_loop_active = true;
    m_is_initialized = true;
}

void Engine::RequestStop()
{
    m_is_render_loop_active = false;
    //m_stop_requested.Set(true, MemoryOrder::RELAXED);
}

void Engine::FinalizeStop()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_MAIN);

    m_is_shutting_down.Set(true, MemoryOrder::SEQUENTIAL);
    m_is_render_loop_active = false;

    HYP_LOG(Engine, LogLevel::INFO, "Stopping all engine processes");

    m_delegates.OnShutdown();

    m_scripting_service->Stop();
    m_scripting_service.Reset();

    // must stop before net request thread
    StopProfilerConnectionThread();

    if (m_net_request_thread != nullptr) {
        if (m_net_request_thread->IsRunning()) {
            m_net_request_thread->Stop();
        }

        if (m_net_request_thread->CanJoin()) {
            m_net_request_thread->Join();
        }
    }

    m_world.Reset();

    if (TaskSystem::GetInstance().IsRunning()) { // Stop task system
        HYP_LOG(Tasks, LogLevel::INFO, "Stopping task system");

        TaskSystem::GetInstance().Stop();

        HYP_LOG(Tasks, LogLevel::INFO, "Task system stopped");
    }

    m_deferred_renderer->Destroy();
    m_deferred_renderer.Reset();

    m_final_pass.Reset();

    m_debug_drawer.Destroy();

    m_render_data->Destroy();

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Destroy(m_instance->GetDevice()));

    m_placeholder_data->Destroy();

    HYPERION_ASSERT_RESULT(m_instance->GetDevice()->Wait());

    g_safe_deleter->ForceDeleteAll();
    RemoveAllEnqueuedRenderObjectsNow<renderer::Platform::CURRENT>(/* force */true);

    HYPERION_ASSERT_RESULT(m_instance->GetDevice()->Wait());

    HYPERION_ASSERT_RESULT(m_instance->Destroy());
}

HYP_API void Engine::RenderNextFrame(Game *game)
{
    HYP_PROFILE_BEGIN;

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

    Frame *frame = GetGPUInstance()->GetFrameHandler()->GetCurrentFrame();
    HYPERION_ASSERT_RESULT(GetGPUDevice()->GetAsyncCompute()->PrepareForFrame(GetGPUDevice(), frame));

    PreFrameUpdate(frame);

    if (g_should_recreate_swapchain) {
        GetDelegates().OnBeforeSwapchainRecreated();

        HYPERION_ASSERT_RESULT(GetGPUDevice()->Wait());
        HYPERION_ASSERT_RESULT(GetGPUInstance()->RecreateSwapchain());
        HYPERION_ASSERT_RESULT(GetGPUDevice()->Wait());

        HYPERION_ASSERT_RESULT(GetGPUInstance()->GetFrameHandler()->GetCurrentFrame()->RecreateFence(
            GetGPUInstance()->GetDevice()
        ));

        // Need to prepare frame again now that swapchain has been recreated.
        HYPERION_ASSERT_RESULT(GetGPUInstance()->GetFrameHandler()->PrepareFrame(
            GetGPUInstance()->GetDevice(),
            GetGPUInstance()->GetSwapchain()
        ));

        m_deferred_renderer->Resize(GetGPUInstance()->GetSwapchain()->extent);

        // m_deferred_renderer->Destroy();

        // RemoveAllEnqueuedRenderObjectsNow<renderer::Platform::CURRENT>(/* force */false);

        // m_deferred_renderer.Reset(new DeferredRenderer);
        // m_deferred_renderer->Create();

        m_final_pass.Reset(new FinalPass);
        m_final_pass->Create();

        frame = GetGPUInstance()->GetFrameHandler()->GetCurrentFrame();

        GetDelegates().OnAfterSwapchainRecreated();

        g_should_recreate_swapchain = false;
    }

    HYPERION_ASSERT_RESULT(frame->BeginCapture(GetGPUInstance()->GetDevice()));
    
    m_world->PreRender(frame);

    game->OnFrameBegin(frame);

    m_world->Render(frame);

    RenderDeferred(frame);

    m_final_pass->Render(frame);

    HYPERION_ASSERT_RESULT(frame->EndCapture(GetGPUInstance()->GetDevice()));

    UpdateBuffersAndDescriptors((GetGPUInstance()->GetFrameHandler()->GetCurrentFrameIndex()));

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

void Engine::PreFrameUpdate(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    // Add/remove pending descriptor sets before flushing render commands or updating buffers and descriptor sets.
    // otherwise we'll have an issue when render commands expect the descriptor sets to be there.
    m_material_descriptor_set_manager.UpdatePendingDescriptorSets(frame);

    HYPERION_ASSERT_RESULT(renderer::RenderCommands::Flush());

    m_deferred_renderer->GetPostProcessing().PerformUpdates();

    m_material_descriptor_set_manager.Update(frame);

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Update(m_instance->GetDevice(), frame->GetFrameIndex()));

    RenderObjectDeleter<renderer::Platform::CURRENT>::Iterate();

    g_safe_deleter->PerformEnqueuedDeletions();

    ResetRenderState(RENDER_STATE_ACTIVE_ENV_PROBE | RENDER_STATE_SCENE | RENDER_STATE_CAMERA);
}

void Engine::ResetRenderState(RenderStateMask mask)
{
    render_state.Reset(mask);
}

void Engine::UpdateBuffersAndDescriptors(uint32 frame_index)
{
    HYP_SCOPE;

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
}

void Engine::RenderDeferred(Frame *frame)
{
    HYP_SCOPE;

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

        HYP_LOG(Engine, LogLevel::DEBUG, "Num elements for descriptor set {}: {}", ref.GetName(), ref->GetLayout().GetElements().Size());
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
            default:
                HYP_LOG(Engine, LogLevel::ERR, "Unhandled descriptor type %d", layout_it.second.type);
                break;
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
