#include "VoxelConeTracing.hpp"

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion::v2 {

const Extent3D VoxelConeTracing::voxel_map_extent { 256 };
const Extent3D VoxelConeTracing::temporal_image_extent { 32 };

constexpr bool vct_manual_mipmap_generation = true;


#pragma region Render commands

struct RENDER_COMMAND(DestroyVCT) : RenderCommand
{
    VoxelConeTracing &vct;

    RENDER_COMMAND(DestroyVCT)(VoxelConeTracing &vct)
        : vct(vct)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            DescriptorSetRef descriptor_set_globals = g_engine->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetDescriptor(DescriptorKey::VOXEL_IMAGE)
                ->SetElementImageSamplerCombined(
                    0,
                    &g_engine->GetPlaceholderData().GetImageView3D1x1x1R8Storage(),
                    &g_engine->GetPlaceholderData().GetSamplerLinear()
                );

            descriptor_set_globals
                ->GetDescriptor(DescriptorKey::VCT_VOXEL_UAV)
                ->SetElementUAV(0, &g_engine->GetPlaceholderData().GetImageView3D1x1x1R8Storage());

            descriptor_set_globals
                ->GetDescriptor(DescriptorKey::VCT_VOXEL_UNIFORMS)
                ->SetElementBuffer(0, g_engine->GetPlaceholderData().GetOrCreateBuffer(g_engine->GetGPUDevice(), renderer::GPUBufferType::CONSTANT_BUFFER, sizeof(VoxelUniforms)));

            // destroy owned descriptor sets, as well as individual mip imageviews
            if constexpr (vct_manual_mipmap_generation) {
                for (auto &mip_image_view : vct.m_mips[frame_index]) {
                    HYPERION_ASSERT_RESULT(mip_image_view->Destroy(g_engine->GetGPUDevice()));
                }

                vct.m_mips[frame_index].Clear();
            }
        }

        return vct.m_uniform_buffer.Destroy(g_engine->GetGPUDevice());
    }
};

#pragma endregion

VoxelConeTracing::VoxelConeTracing(Params &&params)
    : EngineComponentBase(),
      RenderComponent(5), // render every X frames
      m_params(std::move(params))
{
}

VoxelConeTracing::~VoxelConeTracing()
{
    Teardown();
}

void VoxelConeTracing::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    CreateImagesAndBuffers();
    CreateShader();
    CreateFramebuffer();
    CreateDescriptors();
    CreateComputePipelines();

    m_camera = CreateObject<Camera>(voxel_map_extent.width, voxel_map_extent.height);
    m_camera->SetFramebuffer(m_framebuffer);
    m_camera->SetCameraController(UniquePtr<OrthoCameraController>::Construct(
        -Float(voxel_map_extent[0]) * 0.5f, Float(voxel_map_extent[0]) * 0.5f,
        -Float(voxel_map_extent[1]) * 0.5f, Float(voxel_map_extent[1]) * 0.5f,
        -Float(voxel_map_extent[2]) * 0.5f, Float(voxel_map_extent[2]) * 0.5f
    ));

    InitObject(m_camera);

    m_render_list.SetCamera(m_camera);
    
    SetReady(true);

    OnTeardown([this]() {
        m_camera.Reset();

        m_framebuffer.Reset();
        m_clear_voxels.Reset();
        m_generate_mipmap.Reset();
        
        for (UInt i = 0; i < max_frames_in_flight; i++) {
            SafeRelease(std::move(m_generate_mipmap_descriptor_sets[i]));
        }

        RenderCommands::Push<RENDER_COMMAND(DestroyVCT)>(*this);
        
        HYP_SYNC_RENDER();

        m_voxel_image.Reset();
        m_shader.Reset();
        
        SetReady(false);
    });
}

// called from game thread
void VoxelConeTracing::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    // add all entities from environment scene
    AssertThrow(GetParent()->GetScene() != nullptr);
}

void VoxelConeTracing::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    GetParent()->GetScene()->CollectEntities(
        m_render_list,
        m_camera,
        RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::NONE,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
            },
            m_shader->GetCompiledShader().GetDefinition()
        ),
        true // skip culling
    );

    m_render_list.UpdateRenderGroups();
}

void VoxelConeTracing::OnRender(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!g_engine->GetConfig().Get(CONFIG_VOXEL_GI)) {
        return;
    }

    CommandBuffer *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    auto result = renderer::Result::OK;

    /* put our voxel map in an optimal state to be written to */
    m_voxel_image->GetImage()->GetGPUImage()->InsertBarrier(
        command_buffer,
        renderer::ResourceState::UNORDERED_ACCESS
    );

    /* clear the voxels */
    m_clear_voxels->GetPipeline()->Bind(command_buffer);

    command_buffer->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_clear_voxels->GetPipeline(),
        DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
    );

    m_clear_voxels->GetPipeline()->Dispatch(command_buffer, m_voxel_image->GetExtent() / Extent3D { 8, 8, 8 });

    g_engine->GetRenderState().BindScene(GetParent()->GetScene());

    m_render_list.CollectDrawCalls(
        frame,
        Bitset((1 << BUCKET_OPAQUE)),
        nullptr
    );

    m_render_list.ExecuteDrawCalls(
        frame,
        Bitset((1 << BUCKET_OPAQUE)),
        nullptr
    );

    g_engine->GetRenderState().UnbindScene();
    
    if constexpr (vct_manual_mipmap_generation) {
        const UInt num_mip_levels = m_voxel_image->GetImage()->NumMipmaps();
        const Extent3D &voxel_image_extent = m_voxel_image->GetImage()->GetExtent();
        Extent3D mip_extent = voxel_image_extent;

        for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            const Extent3D prev_mip_extent = mip_extent;

            mip_extent.width = MathUtil::Max(1u, voxel_image_extent.width >> mip_level);
            mip_extent.height = MathUtil::Max(1u, voxel_image_extent.height >> mip_level);
            mip_extent.depth = MathUtil::Max(1u, voxel_image_extent.depth >> mip_level);
        
            if (mip_level != 0) {
                // put the mip into writeable state
                m_voxel_image->GetImage()->GetGPUImage()->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::UNORDERED_ACCESS
                );
            }

            command_buffer->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_generate_mipmap->GetPipeline(),
                m_generate_mipmap_descriptor_sets[frame_index][mip_level],
                static_cast<DescriptorSet::Index>(0)
            );

            m_generate_mipmap->GetPipeline()->Bind(
                command_buffer,
                Pipeline::PushConstantData {
                    .voxel_mip_data = {
                        .mip_dimensions = renderer::ShaderVec4<UInt32>(Vec3u(mip_extent), 0),
                        .prev_mip_dimensions = renderer::ShaderVec4<UInt32>(Vec3u(prev_mip_extent), 0),
                        .mip_level = mip_level
                    }
                }
            );
            
            // dispatch to generate this mip level
            m_generate_mipmap->GetPipeline()->Dispatch(
                command_buffer,
                Extent3D {
                    (mip_extent.width + 7) / 8,
                    (mip_extent.height + 7) / 8,
                    (mip_extent.depth + 7) / 8
                }
            );

            // put this mip into readable state
            m_voxel_image->GetImage()->GetGPUImage()->InsertSubResourceBarrier(
                command_buffer,
                renderer::ImageSubResource { .base_mip_level = mip_level },
                renderer::ResourceState::SHADER_RESOURCE
            );
        }

        // all mip levels have been transitioned into this state
        m_voxel_image->GetImage()->GetGPUImage()->SetResourceState(
            renderer::ResourceState::SHADER_RESOURCE
        );
    } else {

        /* unset our state */
        m_voxel_image->GetImage()->GetGPUImage()->InsertBarrier(
            command_buffer,
            renderer::ResourceState::COPY_DST
        );

        /* finally, generate mipmaps. we go through GetImage() because we want to
        * directly call the renderer functions rather than enqueueing a command.
        */
        HYPERION_PASS_ERRORS(
            m_voxel_image->GetImage()->GenerateMipmaps(g_engine->GetGPUDevice(), command_buffer),
            result
        );

        m_voxel_image->GetImage()->GetGPUImage()->InsertBarrier(
            command_buffer,
            renderer::ResourceState::SHADER_RESOURCE
        );
    }

    HYPERION_ASSERT_RESULT(result);
}

void VoxelConeTracing::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    //m_shadow_pass.SetShadowMapIndex(new_index);
    AssertThrowMsg(false, "Not implemented");

    // TODO: Remove descriptor, set new descriptor
}

void VoxelConeTracing::CreateImagesAndBuffers()
{
    m_voxel_image = CreateObject<Texture>(
        StorageImage(
            voxel_map_extent,
            InternalFormat::RGBA8,
            ImageType::TEXTURE_TYPE_3D,
            FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
        ),
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER
    );

    InitObject(m_voxel_image);

    if constexpr (vct_manual_mipmap_generation) {
        const auto num_mip_levels = m_voxel_image->GetImage()->NumMipmaps();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_mips[frame_index].Reserve(num_mip_levels);

            for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
                // do not Create() it yet. Do that in render thread
                m_mips[frame_index].PushBack(std::make_unique<ImageView>());
            }
        }
    }

    struct RENDER_COMMAND(CreateVCTImagesAndBuffers) : RenderCommand
    {
        VoxelConeTracing &vct;

        RENDER_COMMAND(CreateVCTImagesAndBuffers)(VoxelConeTracing &vct)
            : vct(vct)
        {
        }

        virtual Result operator()()
        {
            HYPERION_BUBBLE_ERRORS(vct.m_uniform_buffer.Create(g_engine->GetGPUDevice(), sizeof(VoxelUniforms)));

            VoxelUniforms uniforms { };
            uniforms.extent = Vector4(vct.m_params.aabb.GetExtent(), 0.0f);
            uniforms.aabb_max = Vector4(vct.m_params.aabb.max, 0.0f);
            uniforms.aabb_min = Vector4(vct.m_params.aabb.min, 0.0f);
            uniforms.dimensions = { voxel_map_extent.width, voxel_map_extent.height, voxel_map_extent.depth, vct.m_voxel_image->GetImage()->NumMipmaps() };

            vct.m_uniform_buffer.Copy(
                g_engine->GetGPUDevice(),
                sizeof(uniforms),
                &uniforms
            );

            // create image views for each mip level
            if constexpr (vct_manual_mipmap_generation) {
                const auto num_mip_levels = vct.m_voxel_image->GetImage()->NumMipmaps();

                for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                    for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
                        HYPERION_ASSERT_RESULT(vct.m_mips[frame_index][mip_level]->Create(
                            g_engine->GetGPUDevice(),
                            vct.m_voxel_image->GetImage(),
                            mip_level, 1,
                            0, vct.m_voxel_image->GetImage()->NumFaces()
                        ));
                    }
                }
            }

            HYPERION_RETURN_OK;
        }
    };

    RenderCommands::Push<RENDER_COMMAND(CreateVCTImagesAndBuffers)>(*this);
}

void VoxelConeTracing::CreateComputePipelines()
{
    m_clear_voxels = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(VCTClearVoxels))
    );

    InitObject(m_clear_voxels);

    if constexpr (vct_manual_mipmap_generation) {
        m_generate_mipmap = CreateObject<ComputePipeline>(
            g_shader_manager->GetOrCreate(HYP_NAME(VCTGenerateMipmap)),
            Array<DescriptorSetRef> { m_generate_mipmap_descriptor_sets[0].Front() } // only need to pass first to use for layout.
        );

        InitObject(m_generate_mipmap);
    }
}

void VoxelConeTracing::CreateShader()
{
    Name shader_name = HYP_NAME(VCTVoxelizeWithGeometryShader);

    if (!g_engine->GetGPUDevice()->GetFeatures().SupportsGeometryShaders()) {
        shader_name = HYP_NAME(VCTVoxelizeWithoutGeometryShader);
    }

    m_shader = g_shader_manager->GetOrCreate(
        shader_name,
        ShaderProperties(renderer::static_mesh_vertex_attributes, { "MODE_TEXTURE_3D" })
    );

    AssertThrow(InitObject(m_shader));
}

void VoxelConeTracing::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        Extent2D(voxel_map_extent),
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    InitObject(m_framebuffer);
}

void VoxelConeTracing::CreateDescriptors()
{
    // create own descriptor sets
    if constexpr (vct_manual_mipmap_generation) {
        const auto num_mip_levels = m_voxel_image->GetImage()->NumMipmaps();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_mips[frame_index].Reserve(num_mip_levels);

            for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
                // create descriptor sets for mip generation.
                auto mip_descriptor_set = RenderObjects::Make<renderer::DescriptorSet>();

                auto *mip_in = mip_descriptor_set
                    ->AddDescriptor<renderer::ImageDescriptor>(0);

                if (mip_level == 0) {
                    // first mip level -- input is the actual image
                    mip_in->SetElementSRV(0, m_voxel_image->GetImageView());
                } else {
                    mip_in->SetElementSRV(0, m_mips[frame_index][mip_level - 1].get());
                }

                auto *mip_out = mip_descriptor_set
                    ->AddDescriptor<renderer::StorageImageDescriptor>(1);

                mip_out->SetElementUAV(0, m_mips[frame_index][mip_level].get());

                mip_descriptor_set
                    ->AddDescriptor<renderer::SamplerDescriptor>(2)
                    ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());

                m_generate_mipmap_descriptor_sets[frame_index].PushBack(std::move(mip_descriptor_set));
            }
        }
    }

    struct RENDER_COMMAND(CreateVCTDescriptors) : RenderCommand
    {
        VoxelConeTracing &vct;

        RENDER_COMMAND(CreateVCTDescriptors)(VoxelConeTracing &vct)
            : vct(vct)
        {
        }

        virtual Result operator()()
        {
            DebugLog(LogType::Debug, "Add voxel cone tracing descriptors\n");
            
            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                DescriptorSetRef descriptor_set_globals = g_engine->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

                descriptor_set_globals
                    ->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::VOXEL_IMAGE)
                    ->SetElementImageSamplerCombined(0, vct.m_voxel_image->GetImageView(), &g_engine->GetPlaceholderData().GetSamplerLinearMipmap());

                descriptor_set_globals
                    ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::VCT_VOXEL_UAV)
                    ->SetElementUAV(0, vct.m_voxel_image->GetImageView());

                descriptor_set_globals
                    ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::VCT_VOXEL_UNIFORMS)
                    ->SetElementBuffer(0, &vct.m_uniform_buffer);

                // initialize our own descriptor sets
                if constexpr (vct_manual_mipmap_generation) {
                    for (auto &mip_descriptor_set : vct.m_generate_mipmap_descriptor_sets[frame_index]) {
                        AssertThrow(mip_descriptor_set != nullptr);

                        HYPERION_ASSERT_RESULT(mip_descriptor_set->Create(
                            g_engine->GetGPUDevice(),
                            &g_engine->GetGPUInstance()->GetDescriptorPool()
                        ));
                    }
                }
            }

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(CreateVCTDescriptors, *this);
}

} // namespace hyperion::v2
