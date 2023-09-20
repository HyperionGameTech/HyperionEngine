#include "Engine.hpp"

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/Compute.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/vct/VoxelConeTracing.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <scene/controllers/AabbDebugController.hpp>
#include <scene/controllers/AnimationController.hpp>
#include <scene/controllers/AudioController.hpp>
#include <scene/controllers/ScriptedController.hpp>
#include <scene/controllers/paging/BasicPagingController.hpp>
#include <scene/terrain/controllers/TerrainPagingController.hpp>
#include <scene/controllers/FollowCameraController.hpp>
#include <scene/skydome/controllers/SkydomeController.hpp>
#include <scene/controllers/physics/RigidBodyController.hpp>
#include <ui/controllers/UIButtonController.hpp>

#include <Game.hpp>

#include <util/MeshBuilder.hpp>

#include <audio/AudioManager.hpp>

namespace hyperion::v2 {

using renderer::VertexAttributeSet;
using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;
using renderer::DescriptorKey;
using renderer::FillMode;
using renderer::GPUBufferType;
using renderer::GPUBuffer;

Engine *g_engine = nullptr;
AssetManager *g_asset_manager = nullptr;
ShaderManagerSystem *g_shader_manager = nullptr;
MaterialCache *g_material_system = nullptr;

class StaticDescriptorTable
{
    static constexpr SizeType max_static_descriptor_sets = 8;
    static constexpr SizeType max_static_descriptor_sets_per_slot = 16;

public:
    enum Slot
    {
        SLOT_NONE,
        SRV,
        UAV,
        CBUFF,
        SSBO,
        ACCELERATION_STRUCTURE,
        SLOT_MAX
    };

    struct DescriptorDeclaration
    {
        Slot slot = SLOT_NONE;
        UInt slot_index = ~0u;
        const char *name;

        SizeType GetFlatIndex() const
        {
            return ((SizeType(slot) - 1) * max_static_descriptor_sets_per_slot) + slot_index;
        }
    };

    struct DescriptorSetDeclaration
    {
        UInt set_index = ~0u;
        const char *name;

        DescriptorDeclaration slots[SLOT_MAX][max_static_descriptor_sets_per_slot] = { };
    };

private:
    static DescriptorSetDeclaration declarations[max_static_descriptor_sets];

public:

    struct DeclareSet
    {
        DeclareSet(UInt set_index, const char *name)
        {
            AssertThrow(set_index < max_static_descriptor_sets);

            DescriptorSetDeclaration decl;
            decl.set_index = set_index;
            decl.name = name;

            AssertThrow(declarations[set_index].set_index == ~0u);

            declarations[set_index] = decl;
        }
    };

    struct DeclareDescriptor
    {
        DeclareDescriptor(UInt set_index, Slot slot_type, UInt slot_index, const char *name)
        {
            AssertThrow(set_index < max_static_descriptor_sets);

            AssertThrow(slot_type != SLOT_NONE);
            AssertThrow(slot_index < max_static_descriptor_sets_per_slot);

            DescriptorSetDeclaration &decl = declarations[set_index];
        
            AssertThrow(decl.set_index == set_index);
            AssertThrow(decl.slots[UInt(slot_type) - 1][slot_index].slot_index == ~0u);

            DescriptorDeclaration descriptor_decl;
            descriptor_decl.slot_index = slot_index;
            descriptor_decl.slot = slot_type;
            descriptor_decl.name = name;

            decl.slots[UInt(slot_type) - 1][slot_index] = descriptor_decl;
        }
    };
};

StaticDescriptorTable::DescriptorSetDeclaration StaticDescriptorTable::declarations[max_static_descriptor_sets] = { };

#define DECLARE_DESCRIPTOR_SET(index, name) \
    StaticDescriptorTable::DeclareSet desc_##name(index, HYP_STR(name))

#define DECLARE_DESCRIPTOR_SRV(set_index, slot_index, name) \
    StaticDescriptorTable::DeclareDescriptor desc_##name(set_index, StaticDescriptorTable::Slot::SRV, slot_index, HYP_STR(name))
#define DECLARE_DESCRIPTOR_UAV(set_index, slot_index, name) \
    StaticDescriptorTable::DeclareDescriptor desc_##name(set_index, StaticDescriptorTable::Slot::UAV, slot_index, HYP_STR(name))
#define DECLARE_DESCRIPTOR_CBUFF(set_index, slot_index, name) \
    StaticDescriptorTable::DeclareDescriptor desc_##name(set_index, StaticDescriptorTable::Slot::CBUFF, slot_index, HYP_STR(name))
#define DECLARE_DESCRIPTOR_SSBO(set_index, slot_index, name) \
    StaticDescriptorTable::DeclareDescriptor desc_##name(set_index, StaticDescriptorTable::Slot::SSBO, slot_index, HYP_STR(name))
#define DECLARE_DESCRIPTOR_ACCELERATION_STRUCTURE(set_index, slot_index, name) \
    StaticDescriptorTable::DeclareDescriptor desc_##name(set_index, StaticDescriptorTable::Slot::ACCELERATION_STRUCTURE, slot_index, HYP_STR(name))

DECLARE_DESCRIPTOR_SET(0, Globals);
DECLARE_DESCRIPTOR_SRV(0, 0, Foo);
DECLARE_DESCRIPTOR_UAV(0, 0, Foo1);

DECLARE_DESCRIPTOR_SET(1, Scene);
DECLARE_DESCRIPTOR_SET(2, Object);
DECLARE_DESCRIPTOR_SET(3, Material);

#pragma region Render commands

struct RENDER_COMMAND(CopyBackbufferToCPU) : RenderCommand
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

void StreamerData::Create()
{
    m_texture = CreateObject<Texture>(Texture2D(
        Extent2D { 256, 256 },
        InternalFormat::RGBA8,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    AssertThrow(InitObject(m_texture));

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_buffers[frame_index] = RenderObjects::Make<GPUBuffer>(GPUBufferType::STAGING_BUFFER);

        HYPERION_ASSERT_RESULT(m_buffers[frame_index]->Create(
            g_engine->GetGPUDevice(),
            m_texture->GetImage()->GetByteSize()
        ));
    }
}

void StreamerData::CopyBackbufferImage(Frame *frame, const ImageRef &backbuffer_image)
{
    // ### TEMP: Copy backbuffer to a texture which will be sent to the connected client

    const ImageRef &result_image = m_texture->GetImage();

    // put src image in state for copying from
    backbuffer_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
    // put dst image in state for copying to
    result_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

    // Blit into the streamer image
    result_image->Blit(
        frame->GetCommandBuffer(),
        backbuffer_image
    );

    HYPERION_ASSERT_RESULT(result_image->GenerateMipmaps(
        g_engine->GetGPUDevice(),
        frame->GetCommandBuffer()
    ));

    // put src image in state for reading
    backbuffer_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);

    result_image->CopyToBuffer(frame->GetCommandBuffer(), m_buffers[frame->GetFrameIndex()]);
}

Engine::Engine()
    : shader_globals(nullptr)
{
    RegisterComponents();
}

Engine::~Engine()
{
    m_placeholder_data.Destroy();
    m_immediate_mode.Destroy();

    HYP_SYNC_RENDER(); // just to clear anything remaining up 

    AssertThrow(m_instance != nullptr);
    (void)m_instance->GetDevice()->Wait();

    if (shader_globals != nullptr) {
        shader_globals->Destroy();

        delete shader_globals;
    }

    m_instance->Destroy();
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

void Engine::RegisterComponents()
{
    m_components.Register<AABBDebugController>();
    m_components.Register<TerrainPagingController>();
    m_components.Register<SkydomeController>();
    m_components.Register<ScriptedController>();
    m_components.Register<BasicCharacterController>();
    m_components.Register<AnimationController>();
    m_components.Register<AudioController>();
    m_components.Register<RigidBodyController>();
    m_components.Register<BasicPagingController>();
}

void Engine::FindTextureFormatDefaults()
{
    Threads::AssertOnThread(THREAD_RENDER);

    const Device *device = m_instance->GetDevice();

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_COLOR,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::BGRA8_SRGB,
                        InternalFormat::RGBA16F,
                        InternalFormat::RGBA32F,
                        InternalFormat::RGBA16,
                        InternalFormat::RGBA8 },
            renderer::ImageSupportType::SRV
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::DEPTH_24,
                        InternalFormat::DEPTH_16,
                        InternalFormat::DEPTH_32F },
            renderer::ImageSupportType::DEPTH
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::R10G10B10A2,
                        InternalFormat::RGBA16F,
                        InternalFormat::RGBA32F },
            renderer::ImageSupportType::SRV
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_NORMALS,
        device->GetFeatures().FindSupportedFormat(
            std::array{ //InternalFormat::RG16,
                        InternalFormat::RGBA16F,
                        InternalFormat::RGBA32F,
                        InternalFormat::RGBA8 },
            renderer::ImageSupportType::SRV
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_UV,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::RG16F,
                        InternalFormat::RG32F },
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

void Engine::PrepareFinalPass()
{
    m_full_screen_quad = MeshBuilder::Quad();
    AssertThrow(InitObject(m_full_screen_quad));

    ShaderProperties final_output_props;
    final_output_props.Set("TEMPORAL_AA", GetConfig().Get(CONFIG_TEMPORAL_AA));

    if (GetConfig().Get(CONFIG_DEBUG_SSR)) {
        final_output_props.Set("DEBUG_SSR");
    } else if (GetConfig().Get(CONFIG_DEBUG_HBAO)) {
        final_output_props.Set("DEBUG_HBAO");
    } else if (GetConfig().Get(CONFIG_DEBUG_HBIL)) {
        final_output_props.Set("DEBUG_HBIL");
    } else if (GetConfig().Get(CONFIG_DEBUG_REFLECTIONS)) {
        final_output_props.Set("DEBUG_REFLECTIONS");
    } else if (GetConfig().Get(CONFIG_DEBUG_IRRADIANCE)) {
        final_output_props.Set("DEBUG_IRRADIANCE");
    } else if (GetConfig().Get(CONFIG_PATHTRACER)) {
        final_output_props.Set("PATHTRACER");
    }

    final_output_props.Set("OUTPUT_SRGB", renderer::IsSRGBFormat(m_instance->swapchain->image_format));

    auto shader = g_shader_manager->GetOrCreate(
        HYP_NAME(FinalOutput),
        final_output_props
    );

    AssertThrow(InitObject(shader));

    UInt iteration = 0;

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        RenderObjects::Make<Image>(renderer::FramebufferImage2D(
            m_instance->swapchain->extent,
            m_instance->swapchain->image_format,
            nullptr
        )),
        renderer::RenderPassStage::PRESENT
    ));

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        RenderObjects::Make<Image>(renderer::FramebufferImage2D(
            m_instance->swapchain->extent,
            m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        )),
        renderer::RenderPassStage::PRESENT
    ));
    
    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(m_instance->GetDevice()));
    }

    for (renderer::PlatformImage img : m_instance->swapchain->images) {
        auto fbo = CreateObject<Framebuffer>(
            m_instance->swapchain->extent,
            renderer::RenderPassStage::PRESENT,
            renderer::RenderPass::Mode::RENDER_PASS_INLINE
        );

        renderer::AttachmentUsage *color_attachment_usage,
            *depth_attachment_usage;

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[0]->AddAttachmentUsage(
            m_instance->GetDevice(),
            img,
            renderer::helpers::ToVkFormat(m_instance->swapchain->image_format),
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D,
            1, 1,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &color_attachment_usage
        ));

        color_attachment_usage->SetBinding(0);

        fbo->AddAttachmentUsage(color_attachment_usage);

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[1]->AddAttachmentUsage(
            m_instance->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &depth_attachment_usage
        ));

        fbo->AddAttachmentUsage(depth_attachment_usage);

        depth_attachment_usage->SetBinding(1);

        if (iteration == 0) {
            m_root_pipeline = CreateObject<RenderGroup>(
                std::move(shader),
                RenderableAttributeSet(
                    MeshAttributes {
                        .vertex_attributes = renderer::static_mesh_vertex_attributes
                    },
                    MaterialAttributes {
                        .bucket = BUCKET_SWAPCHAIN
                    }
                )
            );
        }

        m_root_pipeline->AddFramebuffer(std::move(fbo));

        ++iteration;
    }

    callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        m_render_list_container.AddFramebuffersToPipelines();
        InitObject(m_root_pipeline);
    });
}

void Engine::Initialize(RC<Application> application)
{
    Threads::AssertOnThread(THREAD_MAIN);

    RenderCommands::SetOwnerThreadID(Threads::GetThreadID(THREAD_RENDER));

    game_thread.Reset(new GameThread);

    m_crash_handler.Initialize();

    TaskSystem::GetInstance().Start();

#ifdef HYP_WINDOWS
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif

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

    shader_globals = new ShaderGlobals();
    shader_globals->Create();

    m_placeholder_data.Create();

    m_world = CreateObject<World>();
    InitObject(m_world);
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->SetElementBuffer<SceneShaderData>(0, shader_globals->scenes.GetBuffers()[0].get());

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(DescriptorKey::LIGHTS_BUFFER)
        ->SetElementBuffer<LightShaderData>(0, shader_globals->lights.GetBuffer(0).get());

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(DescriptorKey::ENV_GRID_BUFFER)
        ->SetElementBuffer<EnvGridShaderData>(0, shader_globals->env_grids.GetBuffer(0).get());

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(DescriptorKey::CURRENT_ENV_PROBE)
        ->SetElementBuffer<EnvProbeShaderData>(0, shader_globals->env_probes.GetBuffer(0).get());

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(DescriptorKey::CAMERA_BUFFER)
        ->SetElementBuffer<CameraShaderData>(0, shader_globals->cameras.GetBuffer(0).get());

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::SHADOW_MATRICES)
        ->SetElementBuffer(0, shader_globals->shadow_map_data.GetBuffer(0).get());
    
    if constexpr (use_indexed_array_for_object_data) {
        m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
            ->AddDescriptor<renderer::StorageBufferDescriptor>(0)
            ->SetElementBuffer(0, shader_globals->materials.GetBuffers()[0].get());
    } else {
        m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
            ->SetElementBuffer<MaterialShaderData>(0, shader_globals->materials.GetBuffers()[0].get());
    }

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::StorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .buffer = shader_globals->objects.GetBuffers()[0].get()
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(2)
        ->SetSubDescriptor({
            .buffer = shader_globals->skeletons.GetBuffers()[0].get(),
            .range = static_cast<UInt>(sizeof(SkeletonShaderData))
        });


    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->SetElementBuffer<SceneShaderData>(0, shader_globals->scenes.GetBuffers()[1].get());

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(DescriptorKey::LIGHTS_BUFFER)
        ->SetElementBuffer<LightShaderData>(0, shader_globals->lights.GetBuffer(1).get());
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(DescriptorKey::ENV_GRID_BUFFER)
        ->SetElementBuffer<EnvGridShaderData>(0, shader_globals->env_grids.GetBuffer(1).get());
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(DescriptorKey::CURRENT_ENV_PROBE)
        ->SetElementBuffer<EnvProbeShaderData>(0, shader_globals->env_probes.GetBuffer(1).get());

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(DescriptorKey::CAMERA_BUFFER)
        ->SetElementBuffer<CameraShaderData>(0, shader_globals->cameras.GetBuffer(1).get());

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::SHADOW_MATRICES)
        ->SetElementBuffer(0, shader_globals->shadow_map_data.GetBuffer(1).get());
    
    if constexpr (use_indexed_array_for_object_data) {
        m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
            ->AddDescriptor<renderer::StorageBufferDescriptor>(0)
            ->SetElementBuffer(0, shader_globals->materials.GetBuffers()[1].get());
    } else {
        m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
            ->SetElementBuffer<MaterialShaderData>(0, shader_globals->materials.GetBuffers()[1].get());
    }

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::StorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .buffer = shader_globals->objects.GetBuffers()[1].get()
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(2)
        ->SetSubDescriptor({
            .buffer = shader_globals->skeletons.GetBuffers()[1].get(),
            .range = static_cast<UInt>(sizeof(SkeletonShaderData))
        });

#if HYP_FEATURES_BINDLESS_TEXTURES
    m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS)
        ->AddDescriptor<renderer::ImageSamplerDescriptor>(0);

    m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1)
        ->AddDescriptor<renderer::ImageSamplerDescriptor>(0);
#else
    auto *material_sampler_descriptor = m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES)
        ->AddDescriptor<renderer::SamplerDescriptor>(renderer::DescriptorKey::SAMPLER);

    material_sampler_descriptor->SetSubDescriptor({
        .sampler = &GetPlaceholderData().GetSamplerLinear()
    });

    auto *material_textures_descriptor = m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES)
        ->AddDescriptor<renderer::ImageDescriptor>(renderer::DescriptorKey::TEXTURES);

    for (UInt i = 0; i < DescriptorSet::max_material_texture_samplers; i++) {
        material_textures_descriptor->SetSubDescriptor({
            .element_index = i,
            .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
        });
    }
#endif

    for (UInt frame_index = 0; frame_index < UInt(std::size(DescriptorSet::global_buffer_mapping)); frame_index++) {
        const auto descriptor_set_index = DescriptorSet::global_buffer_mapping[frame_index];

        auto *descriptor_set = GetGPUInstance()->GetDescriptorPool()
            .GetDescriptorSet(descriptor_set_index);

        auto *env_probe_textures_descriptor = descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::ENV_PROBE_TEXTURES);

        for (UInt env_probe_index = 0; env_probe_index < max_bound_reflection_probes; env_probe_index++) {
            env_probe_textures_descriptor->SetElementSRV(env_probe_index, &GetPlaceholderData().GetImageViewCube1x1R8());
        }

        descriptor_set
            ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::ENV_PROBES)
            ->SetElementBuffer(0, shader_globals->env_probes.GetBuffers()[frame_index].get());

        auto *point_shadow_maps_descriptor = descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::POINT_SHADOW_MAPS);

        for (UInt shadow_map_index = 0; shadow_map_index < max_bound_point_shadow_maps; shadow_map_index++) {
            point_shadow_maps_descriptor->SetElementSRV(shadow_map_index, &GetPlaceholderData().GetImageViewCube1x1R8());
        }

        // ssr result image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_RESULT)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // ssao/gi combined result image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSAO_GI_RESULT)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // ui placeholder image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::UI_TEXTURE)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // motion vectors result image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::MOTION_VECTORS_RESULT)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // placeholder rt radiance image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::RT_RADIANCE_RESULT)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // placeholder rt probe system uniforms
        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::RT_PROBE_UNIFORMS)
            ->SetSubDescriptor({
                .element_index = 0,
                .buffer = GetPlaceholderData().GetOrCreateBuffer<UniformBuffer>(GetGPUDevice(), sizeof(ProbeSystemUniforms))
            });

        // placeholder rt probes irradiance image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::RT_IRRADIANCE_GRID)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // placeholder rt probes irradiance image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::RT_DEPTH_GRID)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::TEMPORAL_AA_RESULT)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // descriptor_set
        //     ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SH_VOLUMES)
        //     ->SetElementSRV(0, shader_globals->spherical_harmonics_grid.textures[0].image_view)
        //     ->SetElementSRV(1, shader_globals->spherical_harmonics_grid.textures[1].image_view)
        //     ->SetElementSRV(2, shader_globals->spherical_harmonics_grid.textures[2].image_view)
        //     ->SetElementSRV(3, shader_globals->spherical_harmonics_grid.textures[3].image_view)
        //     ->SetElementSRV(4, shader_globals->spherical_harmonics_grid.textures[4].image_view)
        //     ->SetElementSRV(5, shader_globals->spherical_harmonics_grid.textures[5].image_view)
        //     ->SetElementSRV(6, shader_globals->spherical_harmonics_grid.textures[6].image_view)
        //     ->SetElementSRV(7, shader_globals->spherical_harmonics_grid.textures[7].image_view)
        //     ->SetElementSRV(8, shader_globals->spherical_harmonics_grid.textures[8].image_view);

        descriptor_set
            ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::SH_GRID_BUFFER)
            ->SetElementBuffer(0, GetRenderData()->spherical_harmonics_grid.sh_grid_buffer);

        descriptor_set
            ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::VCT_VOXEL_UAV)
            ->SetElementUAV(0, &GetPlaceholderData().GetImageView3D1x1x1R8Storage());

        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::VCT_VOXEL_UNIFORMS)
            ->SetElementBuffer(0, GetPlaceholderData().GetOrCreateBuffer<UniformBuffer>(GetGPUDevice(), sizeof(VoxelUniforms)));

        descriptor_set
            ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::VCT_SVO_BUFFER)
            ->SetElementBuffer(0, GetPlaceholderData().GetOrCreateBuffer<renderer::AtomicCounterBuffer>(GetGPUDevice(), sizeof(UInt32)));

        descriptor_set
            ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::VCT_SVO_FRAGMENT_LIST)
            ->SetElementBuffer(0, GetPlaceholderData().GetOrCreateBuffer<renderer::StorageBuffer>(GetGPUDevice(), sizeof(ShaderVec2<UInt32>)));

        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SH_CLIPMAPS)
            ->SetElementSRV(0, shader_globals->spherical_harmonics_grid.clipmaps[0].image_view)
            ->SetElementSRV(1, shader_globals->spherical_harmonics_grid.clipmaps[1].image_view)
            ->SetElementSRV(2, shader_globals->spherical_harmonics_grid.clipmaps[2].image_view)
            ->SetElementSRV(3, shader_globals->spherical_harmonics_grid.clipmaps[3].image_view)
            ->SetElementSRV(4, shader_globals->spherical_harmonics_grid.clipmaps[4].image_view)
            ->SetElementSRV(5, shader_globals->spherical_harmonics_grid.clipmaps[5].image_view)
            ->SetElementSRV(6, shader_globals->spherical_harmonics_grid.clipmaps[6].image_view)
            ->SetElementSRV(7, shader_globals->spherical_harmonics_grid.clipmaps[7].image_view)
            ->SetElementSRV(8, shader_globals->spherical_harmonics_grid.clipmaps[8].image_view);

        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::LIGHT_FIELD_COLOR_BUFFER)
            ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::LIGHT_FIELD_NORMALS_BUFFER)
            ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::LIGHT_FIELD_DEPTH_BUFFER)
            ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());
    }

    // add placeholder scene data
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto *descriptor_set = GetGPUInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::scene_buffer_mapping[frame_index]);

        auto *shadow_map_descriptor = descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SHADOW_MAPS);
        
        for (UInt i = 0; i < max_shadow_maps; i++) {
            shadow_map_descriptor->SetElementSRV(i, &GetPlaceholderData().GetImageView2D1x1R8());
        }
    }

    // add placeholder scene data
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto *descriptor_set = GetGPUInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::object_buffer_mapping[frame_index]);

        descriptor_set
            ->GetOrAddDescriptor<renderer::DynamicStorageBufferDescriptor>(DescriptorKey::ENTITY_INSTANCES)
            ->SetElementBuffer<EntityInstanceBatch>(0, shader_globals->entity_instance_batches.GetBuffers()[frame_index].get());
    }

    // add VCT descriptor placeholders
    auto *vct_descriptor_set = GetGPUInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);
    
#if 1
    // voxel image
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(0)
        ->SetSubDescriptor({
            .element_index = 0u,
            .image_view = &GetPlaceholderData().GetImageView3D1x1x1R8Storage()
        });

    // voxel uniforms
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(1)
        ->SetSubDescriptor({
            .element_index = 0u,
            .buffer = GetPlaceholderData().GetOrCreateBuffer<UniformBuffer>(GetGPUDevice(), sizeof(VoxelUniforms))
        });

    // temporal blend image
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(6)
        ->SetSubDescriptor({
            .element_index = 0u,
            .image_view = &GetPlaceholderData().GetImageView3D1x1x1R8Storage()
        });
    // voxel image (texture3D)
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::ImageDescriptor>(7)
        ->SetSubDescriptor({
            .element_index = 0u,
            .image_view = &GetPlaceholderData().GetImageView3D1x1x1R8()
        });
    // voxel sampler
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::SamplerDescriptor>(8)
        ->SetSubDescriptor({
            .element_index = 0u,
            .sampler = &GetPlaceholderData().GetSamplerLinear()
        });

#else // svo tests
    // atomic counter
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(0)
        ->SetSubDescriptor({
            .element_index = 0u,
            .buffer = GetPlaceholderData().GetOrCreateBuffer<renderer::AtomicCounterBuffer>(GetGPUDevice(), sizeof(UInt32))
        });

    // fragment list
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .element_index = 0u,
            .buffer = GetPlaceholderData().GetOrCreateBuffer<renderer::StorageBuffer>(GetGPUDevice(), sizeof(ShaderVec2<UInt32>))
        });
#endif
    
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto *descriptor_set_globals = GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::VOXEL_IMAGE)
            ->SetElementImageSamplerCombined(0, &GetPlaceholderData().GetImageView3D1x1x1R8Storage(), &GetPlaceholderData().GetSamplerLinear());

        // add placeholder SSR image
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_FINAL_TEXTURE)
            ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

        // sparse voxel octree buffer
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::SVO_BUFFER)
            ->SetElementBuffer(0, GetPlaceholderData().GetOrCreateBuffer<renderer::StorageBuffer>(GetGPUDevice(), sizeof(ShaderVec2<UInt32>)));

        { // add placeholder gbuffer textures
            auto *gbuffer_textures = descriptor_set_globals->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::GBUFFER_TEXTURES);

            UInt element_index = 0u;

            // not including depth texture here
            for (UInt attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++) {
                gbuffer_textures->SetElementSRV(element_index, &GetPlaceholderData().GetImageView2D1x1R8());

                ++element_index;
            }

            // add translucent bucket's albedo
            gbuffer_textures->SetElementSRV(element_index, &GetPlaceholderData().GetImageView2D1x1R8());

            ++element_index;
        }

        { // more placeholder gbuffer stuff, different slots
            // depth attachment goes into separate slot
            /* Depth texture */
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::GBUFFER_DEPTH)
                ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

            /* Mip chain */
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::GBUFFER_MIP_CHAIN)
                ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

            /* Gbuffer depth sampler */
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_DEPTH_SAMPLER)
                ->SetElementSampler(0, &GetPlaceholderData().GetSamplerNearest());

            /* Gbuffer sampler */
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_SAMPLER)
                ->SetElementSampler(0, &GetPlaceholderData().GetSamplerLinear());

            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEPTH_PYRAMID_RESULT)
                ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_LIGHTING_DIRECT)
                ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_LIGHTING_AMBIENT)
                ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_IRRADIANCE_ACCUM)
                ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_REFLECTION_PROBE)
                ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());
                
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_RESULT)
                ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());
                
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::BLUE_NOISE_BUFFER);
                
            // descriptor_set_globals
            //     ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DOF_BLUR_HOR)
            //     ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

            // descriptor_set_globals
            //     ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DOF_BLUR_VERT)
            //     ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());

            // descriptor_set_globals
            //     ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DOF_BLUR_BLENDED)
            //     ->SetElementSRV(0, &GetPlaceholderData().GetImageView2D1x1R8());
        }

        { // POST FX processing placeholders
            
            for (const auto descriptor_key : { DescriptorKey::POST_FX_PRE_STACK, DescriptorKey::POST_FX_POST_STACK }) {
                auto *descriptor = descriptor_set_globals->GetOrAddDescriptor<renderer::ImageDescriptor>(descriptor_key);

                for (UInt effect_index = 0; effect_index < 4; effect_index++) {
                    descriptor->SetSubDescriptor({
                        .element_index = effect_index,
                        .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                    });
                }
            }
        }
    }


#if 0//HYP_FEATURES_ENABLE_RAYTRACING
    { // add RT placeholders
        auto *rt_descriptor_set = GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING);

        rt_descriptor_set->GetOrAddDescriptor<renderer::StorageImageDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        rt_descriptor_set->GetOrAddDescriptor<renderer::StorageImageDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        rt_descriptor_set->GetOrAddDescriptor<renderer::StorageImageDescriptor>(3)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });
    }
#endif

    HYPERION_ASSERT_RESULT(m_instance->GetDescriptorPool().Create(m_instance->GetDevice()));

    m_render_list_container.Create();

    // has to be after we create framebuffers
    m_immediate_mode.Create();

    AssertThrowMsg(AudioManager::GetInstance()->Initialize(), "Failed to initialize audio device");

    PrepareFinalPass();

    Compile();

    m_streamer_data.Create();
}

void Engine::Compile()
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        /* Finalize env probes */
        shader_globals->env_probes.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize env grids */
        shader_globals->env_grids.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize shadow maps */
        shader_globals->shadow_map_data.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize lights */
        shader_globals->lights.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize skeletons */
        shader_globals->skeletons.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize materials */
        shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize per-object data */
        shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize scene data */
        shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize camera data */
        shader_globals->cameras.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize immediate draw data */
        shader_globals->immediate_draws.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize instance batch data */
        shader_globals->entity_instance_batches.UpdateBuffer(m_instance->GetDevice(), i);
    }

    callbacks.TriggerPersisted(EngineCallback::CREATE_DESCRIPTOR_SETS, this);
    
    m_deferred_renderer.Create();
    
    /* Finalize descriptor pool */
    HYPERION_ASSERT_RESULT(m_instance->GetDescriptorPool().CreateDescriptorSets(m_instance->GetDevice()));
    DebugLog(
        LogType::Debug,
        "Finalized descriptor pool\n"
    );

    HYP_SYNC_RENDER();

    callbacks.TriggerPersisted(EngineCallback::CREATE_GRAPHICS_PIPELINES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_COMPUTE_PIPELINES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_RAYTRACING_PIPELINES, this);

    HYP_SYNC_RENDER();

    m_is_render_loop_active = true;
}

void Engine::RequestStop()
{
    m_stop_requested.Set(true, MemoryOrder::RELAXED);
}

void Engine::FinalizeStop()
{
    Threads::AssertOnThread(THREAD_MAIN);

    m_full_screen_quad.Reset();

    m_is_stopping = true;
    m_is_render_loop_active = false;
    TaskSystem::GetInstance().Stop();

    HYPERION_ASSERT_RESULT(GetGPUInstance()->GetDevice()->Wait());

    if (game_thread != nullptr) {
        while (game_thread->IsRunning()) {
            HYP_SYNC_RENDER();
        }

        game_thread->Join();
    }

    m_render_list_container.Destroy();
    m_deferred_renderer.Destroy();

    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Destroy(m_instance->GetDevice()));
    }

    m_safe_deleter.ForceReleaseAll();

    HYP_SYNC_RENDER();

    m_render_group_mapping.Clear();

    HYP_SYNC_RENDER();

    HYPERION_ASSERT_RESULT(GetGPUInstance()->GetDevice()->Wait());
}

void Engine::RenderNextFrame(Game *game)
{
    if (m_stop_requested.Get(MemoryOrder::RELAXED)) {
        FinalizeStop();

        return;
    }

    auto frame_result = GetGPUInstance()->GetFrameHandler()->PrepareFrame(
        GetGPUInstance()->GetDevice(),
        GetGPUInstance()->GetSwapchain()
    );

    if (!frame_result) {
        m_crash_handler.HandleGPUCrash(frame_result);

        m_is_render_loop_active = false;
        RequestStop();
    }

    auto *frame = GetGPUInstance()->GetFrameHandler()->GetCurrentFrameData().Get<Frame>();

    PreFrameUpdate(frame);

    HYPERION_ASSERT_RESULT(frame->BeginCapture(GetGPUInstance()->GetDevice()));

    m_world->PreRender(frame);

    game->OnFrameBegin(frame);

    m_world->Render(frame);

    RenderDeferred(frame);
    RenderFinalPass(frame);

    HYPERION_ASSERT_RESULT(frame->EndCapture(GetGPUInstance()->GetDevice()));

    frame_result = frame->Submit(&GetGPUInstance()->GetGraphicsQueue());

    if (!frame_result) {
        m_crash_handler.HandleGPUCrash(frame_result);

        m_is_render_loop_active = false;
        RequestStop();
    }

    game->OnFrameEnd(frame);

    GetGPUInstance()->GetFrameHandler()->PresentFrame(&GetGPUInstance()->GetGraphicsQueue(), GetGPUInstance()->GetSwapchain());
    GetGPUInstance()->GetFrameHandler()->NextFrame();
}

Handle<RenderGroup> Engine::CreateRenderGroup(const RenderableAttributeSet &renderable_attributes)
{
    Handle<Shader> shader = g_shader_manager->GetOrCreate(renderable_attributes.GetShaderDefinition());

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
        Threads::CurrentThreadID().name.Data()
    );

    std::lock_guard guard(m_render_group_mapping_mutex);

    AddRenderGroupInternal(renderer_instance, false);

    return renderer_instance;
}

Handle<RenderGroup> Engine::CreateRenderGroup(
    const Handle<Shader> &shader,
    const RenderableAttributeSet &renderable_attributes,
    const Array<const DescriptorSet *> &used_descriptor_sets
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
        used_descriptor_sets
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

    //if (RenderCommands::Count() != 0) {
        HYPERION_ASSERT_RESULT(RenderCommands::Flush());
    //}

    UpdateBuffersAndDescriptors(frame->GetFrameIndex());

    ResetRenderState(RENDER_STATE_ACTIVE_ENV_PROBE | RENDER_STATE_VISIBILITY | RENDER_STATE_SCENE | RENDER_STATE_CAMERA);
}

void Engine::ResetRenderState(RenderStateMask mask)
{
    render_state.Reset(mask);
}

void Engine::UpdateBuffersAndDescriptors(UInt frame_index)
{
    shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->cameras.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->skeletons.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->lights.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->shadow_map_data.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->env_probes.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->env_grids.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->immediate_draws.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->entity_instance_batches.UpdateBuffer(m_instance->GetDevice(), frame_index);

    m_deferred_renderer.GetPostProcessing().PerformUpdates();
    
    m_instance->GetDescriptorPool().AddPendingDescriptorSets(m_instance->GetDevice(), frame_index);
    m_instance->GetDescriptorPool().DestroyPendingDescriptorSets(m_instance->GetDevice(), frame_index);
    m_instance->GetDescriptorPool().UpdateDescriptorSets(m_instance->GetDevice(), frame_index);

    RenderObjectDeleter::Iterate();

    m_safe_deleter.PerformEnqueuedDeletions();
}

void Engine::RenderDeferred(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_deferred_renderer.Render(frame, render_state.GetScene().render_environment);
}

void Engine::RenderFinalPass(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const GraphicsPipelineRef &pipeline = m_root_pipeline->GetPipeline();
    const UInt acquired_image_index = m_instance->GetFrameHandler()->GetAcquiredImageIndex();

    m_root_pipeline->GetFramebuffers()[acquired_image_index]->BeginCapture(0, frame->GetCommandBuffer());
    
    pipeline->Bind(frame->GetCommandBuffer());

    m_instance->GetDescriptorPool().Bind(
        m_instance->GetDevice(),
        frame->GetCommandBuffer(),
        pipeline,
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

#if HYP_FEATURES_ENABLE_RAYTRACING && HYP_FEATURES_BINDLESS_TEXTURES
    /* TMP */
    m_instance->GetDescriptorPool().Bind(
        m_instance->GetDevice(),
        frame->GetCommandBuffer(),
        pipeline,
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
            .count = 1
        }}
    );
#endif

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    m_full_screen_quad->Render(frame->GetCommandBuffer());
    
    m_root_pipeline->GetFramebuffers()[acquired_image_index]->EndCapture(0, frame->GetCommandBuffer());

    // TEMP
    m_streamer_data.CopyBackbufferImage(
        frame,
        m_root_pipeline->GetFramebuffers()[acquired_image_index]->GetAttachmentUsages()[0]->GetAttachment()->GetImage()
    );
}
} // namespace hyperion::v2
