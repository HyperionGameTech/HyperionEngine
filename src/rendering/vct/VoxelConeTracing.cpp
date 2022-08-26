#include "VoxelConeTracing.hpp"

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>
#include <camera/OrthoCamera.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion::v2 {

const Extent3D VoxelConeTracing::voxel_map_size { 256 };

VoxelConeTracing::VoxelConeTracing(Params &&params)
    : EngineComponentBase(),
      RenderComponent(25), // render every 25 frames
      m_params(std::move(params))
{
}

VoxelConeTracing::~VoxelConeTracing()
{
    Teardown();
}

void VoxelConeTracing::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ANY, [this](...) {
        auto *engine = GetEngine();

        m_scene = Handle<Scene>(new Scene(
            Handle<Camera>(new OrthoCamera(
                voxel_map_size.width, voxel_map_size.height,
                -static_cast<float>(voxel_map_size[0]) * 0.5f, static_cast<float>(voxel_map_size[0]) * 0.5f,
                -static_cast<float>(voxel_map_size[1]) * 0.5f, static_cast<float>(voxel_map_size[1]) * 0.5f,
                -static_cast<float>(voxel_map_size[2]) * 0.5f, static_cast<float>(voxel_map_size[2]) * 0.5f
            ))
        ));

        Attach(m_scene);

        CreateImagesAndBuffers(engine);
        CreateShader(engine);
        CreateRenderPass(engine);
        CreateFramebuffers(engine);
        CreateRendererInstance(engine);
        CreateDescriptors(engine);
        CreateComputePipelines(engine);
        
        SetReady(true);

        OnTeardown([this]() {
            auto *engine = GetEngine();

            engine->GetWorld().RemoveScene(m_scene->GetId());
            m_scene.Reset();

            m_framebuffers = {};
            m_render_pass.Reset();
            m_renderer_instance.Reset();
            m_clear_voxels.Reset();

            engine->render_scheduler.Enqueue([this, engine](...) {

                // unset descriptors

                auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
                    .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

                descriptor_set
                    ->GetDescriptor(0)
                    ->SetSubDescriptor({
                        .element_index = 0u,
                        .image_view = &engine->GetPlaceholderData().GetImageView3D1x1x1R8Storage()
                    });

                descriptor_set
                    ->GetDescriptor(1)
                    ->SetSubDescriptor({
                        .element_index = 0u,
                        .buffer = engine->GetPlaceholderData().GetOrCreateBuffer<UniformBuffer>(engine->GetDevice(), sizeof(VoxelUniforms))
                    });

                for (UInt i = 0; i < max_frames_in_flight; i++) {
                    auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
                    descriptor_set_globals->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::VOXEL_IMAGE)
                        ->SetSubDescriptor({
                            .element_index = 0u,
                            .image_view    = &engine->GetPlaceholderData().GetImageView3D1x1x1R8Storage(),
                            .sampler       = &engine->GetPlaceholderData().GetSamplerLinear()
                        });
                }

                return m_uniform_buffer.Destroy(engine->GetDevice());
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
    
            engine->SafeReleaseRenderResource<Texture>(std::move(m_voxel_image));
            engine->SafeReleaseRenderResource<Shader>(std::move(m_shader));
            
            SetReady(false);
        });
    }));
}

// called from game thread
void VoxelConeTracing::InitGame(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    // add all entities from environment scene
    AssertThrow(GetParent()->GetScene() != nullptr);

    for (auto &it : GetParent()->GetScene()->GetEntities()) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        if (BucketHasGlobalIllumination(entity->GetBucket())
            && (entity->GetRenderableAttributes().mesh_attributes.vertex_attributes
                & m_renderer_instance->GetRenderableAttributes().mesh_attributes.vertex_attributes)) {

            m_renderer_instance->AddEntity(Handle<Entity>(it.second));
        }
    }
}

void VoxelConeTracing::OnEntityAdded(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (BucketHasGlobalIllumination(entity->GetBucket())
        && (entity->GetRenderableAttributes().mesh_attributes.vertex_attributes
            & m_renderer_instance->GetRenderableAttributes().mesh_attributes.vertex_attributes)) {
        m_renderer_instance->AddEntity(Handle<Entity>(entity));
    }
}

void VoxelConeTracing::OnEntityRemoved(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    m_renderer_instance->RemoveEntity(Handle<Entity>(entity));
}

void VoxelConeTracing::OnEntityRenderableAttributesChanged(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    const auto &renderable_attributes = entity->GetRenderableAttributes();

    if (BucketHasGlobalIllumination(entity->GetBucket())
        && (entity->GetRenderableAttributes().mesh_attributes.vertex_attributes
            & m_renderer_instance->GetRenderableAttributes().mesh_attributes.vertex_attributes)) {
        m_renderer_instance->AddEntity(Handle<Entity>(entity));
    } else {
        m_renderer_instance->RemoveEntity(Handle<Entity>(entity));
    }
}

void VoxelConeTracing::OnUpdate(Engine *engine, GameCounter::TickUnit delta)
{
    // Threads::AssertOnThread(THREAD_GAME);
    AssertReady();
}

void VoxelConeTracing::OnRender(Engine *engine, Frame *frame)
{
    // Threads::AssertOnThread(THREAD_RENDER);

    auto *command_buffer = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    auto result = renderer::Result::OK;

    /* put our voxel map in an optimal state to be written to */
    m_voxel_image->GetImage().GetGPUImage()->InsertBarrier(
        command_buffer,
        renderer::GPUMemory::ResourceState::UNORDERED_ACCESS
    );

    /* clear the voxels */
    m_clear_voxels->GetPipeline()->Bind(command_buffer);

    HYPERION_PASS_ERRORS(
        engine->GetInstance()->GetDescriptorPool().Bind(
            engine->GetDevice(),
            command_buffer,
            m_clear_voxels->GetPipeline(),
            {{.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1}}
        ),
        result
    );

    m_clear_voxels->GetPipeline()->Dispatch(command_buffer, m_voxel_image->GetExtent() / Extent3D{8, 8, 8});

    engine->render_state.BindScene(m_scene.Get());

    HYPERION_PASS_ERRORS(
        engine->GetInstance()->GetDescriptorPool().Bind(
            engine->GetDevice(),
            command_buffer,
            m_renderer_instance->GetPipeline(),
            {{.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1}}
        ),
        result
    );

    m_framebuffers[frame_index]->BeginCapture(command_buffer);
    m_renderer_instance->Render(engine, frame);
    m_framebuffers[frame_index]->EndCapture(command_buffer);

    engine->render_state.UnbindScene();

    /* unset our state */
    m_voxel_image->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::COPY_DST);

    /* finally, generate mipmaps. we go through GetImage() because we want to
     * directly call the renderer functions rather than enqueueing a command.
     */
    HYPERION_PASS_ERRORS(
        m_voxel_image->GetImage().GenerateMipmaps(engine->GetDevice(), command_buffer),
        result
    );

    m_voxel_image->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    HYPERION_ASSERT_RESULT(result);
}

void VoxelConeTracing::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    //m_shadow_pass.SetShadowMapIndex(new_index);
    AssertThrowMsg(false, "Not implemented");

    // TODO: Remove descriptor, set new descriptor
}

void VoxelConeTracing::CreateImagesAndBuffers(Engine *engine)
{
    m_voxel_image = Handle<Texture>(new Texture(
        StorageImage(
            voxel_map_size,
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
            Image::Type::TEXTURE_TYPE_3D,
            Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
        ),
        Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER
    ));

    Attach(m_voxel_image);

    engine->render_scheduler.Enqueue([this, engine](...) {
        HYPERION_BUBBLE_ERRORS(m_uniform_buffer.Create(engine->GetDevice(), sizeof(VoxelUniforms)));

        const VoxelUniforms uniforms {
            .extent      = voxel_map_size,
            .aabb_max    = m_params.aabb.GetMax().ToVector4(),
            .aabb_min    = m_params.aabb.GetMin().ToVector4(),
            .num_mipmaps = m_voxel_image->GetImage().NumMipmaps()
        };

        m_uniform_buffer.Copy(
            engine->GetDevice(),
            sizeof(uniforms),
            &uniforms
        );

        HYPERION_RETURN_OK;
    });
}

void VoxelConeTracing::CreateRendererInstance(Engine *engine)
{
    auto renderer_instance = std::make_unique<RendererInstance>(
        std::move(m_shader),
        Handle<RenderPass>(m_render_pass),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes,
                .cull_faces = FaceCullMode::NONE
            },
            MaterialAttributes {
                .bucket = BUCKET_VOXELIZER,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
            }
        )
    );

    for (auto &framebuffer : m_framebuffers) {
        renderer_instance->AddFramebuffer(Handle<Framebuffer>(framebuffer));
    }
    
    m_renderer_instance = engine->AddRendererInstance(std::move(renderer_instance));
    Attach(m_renderer_instance);
}

void VoxelConeTracing::CreateComputePipelines(Engine *engine)
{
    m_clear_voxels = Handle<ComputePipeline>(new ComputePipeline(
        Handle<Shader>(new Shader(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vct/clear_voxels.comp.spv")).Read()}}
            }
        ))
    ));

    Attach(m_clear_voxels);
}

void VoxelConeTracing::CreateShader(Engine *engine)
{
    std::vector<SubShader> sub_shaders = {
        {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/vct/voxelize.vert.spv")).Read()}},
        {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/vct/voxelize.frag.spv")).Read()}}
    };

    if (engine->GetDevice()->GetFeatures().SupportsGeometryShaders()) {
        sub_shaders.push_back({ShaderModule::Type::GEOMETRY, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/vct/voxelize.geom.spv")).Read()}});
    } else {
        DebugLog(
            LogType::Debug,
            "Geometry shaders not supported on device, continuing without adding geometry shader to VCT pipeline.\n"
        );
    }
    
    m_shader = Handle<Shader>(new Shader(sub_shaders));
    Attach(m_shader);
}

void VoxelConeTracing::CreateRenderPass(Engine *engine)
{
    m_render_pass = Handle<RenderPass>(new RenderPass(
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    ));

    Attach(m_render_pass);
}

void VoxelConeTracing::CreateFramebuffers(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = Handle<Framebuffer>(new Framebuffer(
            Extent2D(voxel_map_size),
            Handle<RenderPass>(m_render_pass)
        ));
        
        Attach(m_framebuffers[i]);
    }
}

void VoxelConeTracing::CreateDescriptors(Engine *engine)
{
    DebugLog(LogType::Debug, "Add voxel cone tracing descriptors\n");

    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

    descriptor_set
        ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(0)
        ->SetSubDescriptor({ .element_index = 0u, .image_view = &m_voxel_image->GetImageView() });

    descriptor_set
        ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(1)
        ->SetSubDescriptor({ .element_index = 0u, .buffer = &m_uniform_buffer });
    
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
        descriptor_set_globals->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::VOXEL_IMAGE)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view    = &m_voxel_image->GetImageView(),
                .sampler       = &m_voxel_image->GetSampler()
            });
    }
}

} // namespace hyperion::v2
