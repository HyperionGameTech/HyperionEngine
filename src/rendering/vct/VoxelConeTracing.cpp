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

        m_scene = engine->CreateHandle<Scene>(
            engine->CreateHandle<Camera>(new OrthoCamera(
                voxel_map_size.width, voxel_map_size.height,
                -static_cast<float>(voxel_map_size[0]) * 0.5f, static_cast<float>(voxel_map_size[0]) * 0.5f,
                -static_cast<float>(voxel_map_size[1]) * 0.5f, static_cast<float>(voxel_map_size[1]) * 0.5f,
                -static_cast<float>(voxel_map_size[2]) * 0.5f, static_cast<float>(voxel_map_size[2]) * 0.5f
            ))
        );

        engine->InitObject(m_scene);

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
            m_generate_mipmap.Reset();

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
                            .image_view = &engine->GetPlaceholderData().GetImageView3D1x1x1R8Storage(),
                            .sampler = &engine->GetPlaceholderData().GetSamplerLinear()
                        });

                        // destroy owned descriptor sets, as well as individual mip imageviews
                        if constexpr (manual_mipmap_generation) {
                            for (auto &descriptor_set : m_generate_mipmap_descriptor_sets[i]) {
                                HYPERION_ASSERT_RESULT(descriptor_set->Destroy(engine->GetDevice()));
                            }

                            m_generate_mipmap_descriptor_sets[i].Clear();

                            for (auto &mip_image_view : m_mips[i]) {
                                HYPERION_ASSERT_RESULT(mip_image_view->Destroy(engine->GetDevice()));
                            }

                            m_mips[i].Clear();
                        }
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

    m_clear_voxels->GetPipeline()->Dispatch(command_buffer, m_voxel_image->GetExtent() / Extent3D { 8, 8, 8 });

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

    if constexpr (manual_mipmap_generation) {
        const auto num_mip_levels = m_voxel_image->GetImage().NumMipmaps();
        const auto voxel_image_extent = m_voxel_image->GetImage().GetExtent();
        auto mip_extent = voxel_image_extent;

        for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            const auto prev_mip_extent = mip_extent;

            mip_extent.width = MathUtil::Max(1u, voxel_image_extent.width >> mip_level);
            mip_extent.height = MathUtil::Max(1u, voxel_image_extent.height >> mip_level);
            mip_extent.depth = MathUtil::Max(1u, voxel_image_extent.depth >> mip_level);
        
            if (mip_level != 0) {
                // put the mip into writeable state
                m_voxel_image->GetImage().GetGPUImage()->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::GPUMemory::ResourceState::UNORDERED_ACCESS
                );
            }

            command_buffer->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_generate_mipmap->GetPipeline(),
                m_generate_mipmap_descriptor_sets[frame_index][mip_level].get(),
                static_cast<DescriptorSet::Index>(0)
            );

            m_generate_mipmap->GetPipeline()->Bind(
                command_buffer,
                Pipeline::PushConstantData {
                    .voxel_mip_data = {
                        .mip_dimensions = renderer::ShaderVec4<UInt32>(Vector(mip_extent, 1.0f)),
                        .prev_mip_dimensions = renderer::ShaderVec4<UInt32>(Vector(prev_mip_extent, 1.0f)),
                        .mip_level = mip_level
                    }
                }
            );
            
            // dispatch to generate this mip level
            m_generate_mipmap->GetPipeline()->Dispatch(
                command_buffer,
                mip_extent / Extent3D { 8, 8, 8 }
            );

            // put this mip into readable state
            m_voxel_image->GetImage().GetGPUImage()->InsertSubResourceBarrier(
                command_buffer,
                renderer::ImageSubResource { .base_mip_level = mip_level },
                renderer::GPUMemory::ResourceState::SHADER_RESOURCE
            );
        }

        // all mip levels have been transitioned into this state
        m_voxel_image->GetImage().GetGPUImage()->SetResourceState(
            renderer::GPUMemory::ResourceState::SHADER_RESOURCE
        );
    } else {

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

    }

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
    m_voxel_image = engine->CreateHandle<Texture>(
        StorageImage(
            voxel_map_size,
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
            Image::Type::TEXTURE_TYPE_3D,
            Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
        ),
        Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER
    );

    engine->InitObject(m_voxel_image);

    if constexpr (manual_mipmap_generation) {
        const auto num_mip_levels = m_voxel_image->GetImage().NumMipmaps();

        for (UInt i = 0; i < max_frames_in_flight; i++) {
            m_mips[i].Reserve(num_mip_levels);

            for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
                // do not Create() it yet. Do that in render thread
                m_mips[i].PushBack(std::make_unique<ImageView>());
            }
        }
    }

    engine->render_scheduler.Enqueue([this, engine](...) {
        HYPERION_BUBBLE_ERRORS(m_uniform_buffer.Create(engine->GetDevice(), sizeof(VoxelUniforms)));

        const VoxelUniforms uniforms {
            .extent = voxel_map_size,
            .aabb_max = m_params.aabb.GetMax().ToVector4(),
            .aabb_min = m_params.aabb.GetMin().ToVector4(),
            .num_mipmaps = m_voxel_image->GetImage().NumMipmaps()
        };

        m_uniform_buffer.Copy(
            engine->GetDevice(),
            sizeof(uniforms),
            &uniforms
        );

        // create image views for each mip level
        if constexpr (manual_mipmap_generation) {
            const auto num_mip_levels = m_voxel_image->GetImage().NumMipmaps();

            for (UInt i = 0; i < max_frames_in_flight; i++) {
                for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
                    HYPERION_ASSERT_RESULT(m_mips[i][mip_level]->Create(
                        engine->GetDevice(),
                        &m_voxel_image->GetImage(),
                        mip_level, 1,
                        0, m_voxel_image->GetImage().NumFaces()
                    ));
                }
            }
        }

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
    engine->InitObject(m_renderer_instance);
}

void VoxelConeTracing::CreateComputePipelines(Engine *engine)
{
    m_clear_voxels = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vct/clear_voxels.comp.spv")).Read()}}
            }
        )
    );

    engine->InitObject(m_clear_voxels);

    if constexpr (manual_mipmap_generation) {
        m_generate_mipmap = engine->CreateHandle<ComputePipeline>(
            engine->CreateHandle<Shader>(
                std::vector<SubShader>{
                    { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vct/GenerateMipmap.comp.spv")).Read()}}
                }
            ),
            DynArray<const DescriptorSet *> { m_generate_mipmap_descriptor_sets[0].Front().get() } // only need to pass first to use for layout.
        );

        engine->InitObject(m_generate_mipmap);
    }
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
    
    m_shader = engine->CreateHandle<Shader>(sub_shaders);
    engine->InitObject(m_shader);
}

void VoxelConeTracing::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->CreateHandle<RenderPass>(
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    engine->InitObject(m_render_pass);
}

void VoxelConeTracing::CreateFramebuffers(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->CreateHandle<Framebuffer>(
            Extent2D(voxel_map_size),
            Handle<RenderPass>(m_render_pass)
        );
        
        engine->InitObject(m_framebuffers[i]);
    }
}

void VoxelConeTracing::CreateDescriptors(Engine *engine)
{
    // create own descriptor sets
    if constexpr (manual_mipmap_generation) {
        const auto num_mip_levels = m_voxel_image->GetImage().NumMipmaps();

        for (UInt i = 0; i < max_frames_in_flight; i++) {
            m_mips[i].Reserve(num_mip_levels);

            for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
                // create descriptor sets for mip generation.
                auto mip_descriptor_set = std::make_unique<DescriptorSet>();

                auto *mip_in = mip_descriptor_set
                    ->AddDescriptor<renderer::ImageDescriptor>(0);

                if (mip_level == 0) {
                    // first mip level -- input is the actual image
                    mip_in->SetSubDescriptor({
                        .element_index = 0u,
                        .image_view = &m_voxel_image->GetImageView()
                    });
                } else {
                    mip_in->SetSubDescriptor({
                        .element_index = 0u,
                        .image_view = m_mips[i][mip_level - 1].get()
                    });
                }

                auto *mip_out = mip_descriptor_set
                    ->AddDescriptor<renderer::StorageImageDescriptor>(1);

                mip_out->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = m_mips[i][mip_level].get()
                });

                mip_descriptor_set
                    ->AddDescriptor<renderer::SamplerDescriptor>(2)
                    ->SetSubDescriptor({
                        .sampler = &engine->GetPlaceholderData().GetSamplerLinear()
                    });

                m_generate_mipmap_descriptor_sets[i].PushBack(std::move(mip_descriptor_set));
            }
        }
    }

    engine->render_scheduler.Enqueue([this, engine](...) {
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
                    .image_view = &m_voxel_image->GetImageView(),
                    .sampler = &m_voxel_image->GetSampler()
                });

            // initialize our own descriptor sets
            if constexpr (manual_mipmap_generation) {
                for (auto &mip_descriptor_set : m_generate_mipmap_descriptor_sets[i]) {
                    AssertThrow(mip_descriptor_set != nullptr);

                    HYPERION_ASSERT_RESULT(mip_descriptor_set->Create(
                        engine->GetDevice(),
                        &engine->GetInstance()->GetDescriptorPool()
                    ));
                }
            }
        }

        HYPERION_RETURN_OK;
    });
}

} // namespace hyperion::v2
