#include "vct.h"

#include <util/fs/fs_util.h>

#include <engine.h>
#include <camera/ortho_camera.h>

#include <rendering/backend/renderer_features.h>

namespace hyperion::v2 {

const Extent3D VoxelConeTracing::voxel_map_size{256};

VoxelConeTracing::VoxelConeTracing(Params &&params)
    : EngineComponentBase(),
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

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_VOXELIZER, [this](Engine *engine) {
        m_scene = engine->resources.scenes.Add(std::make_unique<Scene>(
            std::make_unique<OrthoCamera>(
                voxel_map_size.width, voxel_map_size.height,
                -static_cast<float>(voxel_map_size[0]) * 0.5f, static_cast<float>(voxel_map_size[0]) * 0.5f,
                -static_cast<float>(voxel_map_size[1]) * 0.5f, static_cast<float>(voxel_map_size[1]) * 0.5f,
                -static_cast<float>(voxel_map_size[2]) * 0.5f, static_cast<float>(voxel_map_size[2]) * 0.5f
            )
        ));

        CreateImagesAndBuffers(engine);
        CreateShader(engine);
        CreateRenderPass(engine);
        CreateFramebuffer(engine);
        CreateDescriptors(engine);
        CreateGraphicsPipeline(engine);
        CreateComputePipelines(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_VOXELIZER, [this](Engine *engine) {
            m_spatial_observers.Clear();
            m_pipeline_observers.clear();

            m_shader       = nullptr;
            m_framebuffer  = nullptr;
            m_render_pass  = nullptr;
            m_pipeline     = nullptr;
            m_clear_voxels = nullptr;
            m_voxel_image  = nullptr;

            engine->render_scheduler.Enqueue([this, engine](...) {
                return m_uniform_buffer.Destroy(engine->GetDevice());
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        }), engine);
    }));
}

void VoxelConeTracing::RenderVoxels(Engine *engine, Frame *frame)
{
    Engine::AssertOnThread(THREAD_RENDER);

    auto *command_buffer = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    auto result = renderer::Result::OK;

    /* put our voxel map in an optimal state to be written to */
    m_voxel_image->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

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

    engine->render_state.BindScene(m_scene);

    HYPERION_PASS_ERRORS(
        engine->GetInstance()->GetDescriptorPool().Bind(
            engine->GetDevice(),
            command_buffer,
            m_pipeline->GetPipeline(),
            {{.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1}}
        ),
        result
    );

    m_framebuffer->BeginCapture(command_buffer);
    m_pipeline->Render(engine, frame);
    m_framebuffer->EndCapture(command_buffer);

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

void VoxelConeTracing::CreateImagesAndBuffers(Engine *engine)
{
    m_voxel_image = engine->resources.textures.Add(std::make_unique<Texture>(
        StorageImage(
            voxel_map_size,
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
            Image::Type::TEXTURE_TYPE_3D,
            Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
        ),
        Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER
    ));

    m_voxel_image.Init();

    engine->render_scheduler.Enqueue([this, engine](...) {
        HYPERION_BUBBLE_ERRORS(m_uniform_buffer.Create(engine->GetDevice(), sizeof(VoxelUniforms)));

        const VoxelUniforms uniforms{
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

void VoxelConeTracing::CreateGraphicsPipeline(Engine *engine)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(
        std::move(m_shader),
        m_render_pass.IncRef(),
        RenderableAttributeSet{
            .bucket            = BUCKET_VOXELIZER,
            .vertex_attributes = VertexAttributeSet::static_mesh | VertexAttributeSet::skeleton
        }
    );

    pipeline->SetDepthWrite(false);
    pipeline->SetDepthTest(false);
    pipeline->SetFaceCullMode(FaceCullMode::NONE);
    
    pipeline->AddFramebuffer(m_framebuffer.IncRef());
    
    m_pipeline = engine->AddGraphicsPipeline(std::move(pipeline));
    m_pipeline.Init();

    m_pipeline_observers.push_back(engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetGraphicsPipelineNotifier().Add(Observer<Ref<GraphicsPipeline>>(
        [this](Ref<GraphicsPipeline> *pipelines, size_t count) {
            for (size_t i = 0; i < count; i++) {
                if (pipelines[i] == m_pipeline.ptr) {
                    continue;
                }

                m_spatial_observers.Insert(pipelines[i]->GetId(), pipelines[i]->GetSpatialNotifier().Add(Observer<Ref<Spatial>>(
                    [this](Ref<Spatial> *items, size_t count) {
                        for (size_t i = 0; i < count; i++) {
                            m_pipeline->AddSpatial(items[i].IncRef());
                        }
                    },
                    [this](Ref<Spatial> *items, size_t count) {
                        for (size_t i = 0; i < count; i++) {
                            m_pipeline->RemoveSpatial(items[i]->GetId());
                        }
                    }
                )));
            }
        },
        [this](Ref<GraphicsPipeline> *pipelines, size_t count) {
            for (size_t i = 0; i < count; i++) {
                if (pipelines[i] == m_pipeline.ptr) {
                    continue;
                }

                m_spatial_observers.Erase(pipelines[i]->GetId());
            }
        }
    )));
}

void VoxelConeTracing::CreateComputePipelines(Engine *engine)
{
    m_clear_voxels = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vct/clear_voxels.comp.spv")).Read()}}
            }
        ))
    ));

    m_clear_voxels.Init();
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
    
    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(sub_shaders));
    m_shader->Init(engine);
}

void VoxelConeTracing::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->resources.render_passes.Add(std::make_unique<RenderPass>(
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    ));

    m_render_pass.Init();
}

void VoxelConeTracing::CreateFramebuffer(Engine *engine)
{
    m_framebuffer = engine->resources.framebuffers.Add(std::make_unique<Framebuffer>(
        voxel_map_size.ToExtent2D(),
        m_render_pass.IncRef()
    ));
    
    m_framebuffer.Init();
}

void VoxelConeTracing::CreateDescriptors(Engine *engine)
{
    DebugLog(LogType::Debug, "Add voxel cone tracing descriptors\n");
    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

    descriptor_set
        ->AddDescriptor<renderer::StorageImageDescriptor>(0)
        ->AddSubDescriptor({.image_view = &m_voxel_image->GetImageView()});

    descriptor_set
        ->AddDescriptor<renderer::UniformBufferDescriptor>(1)
        ->AddSubDescriptor({.buffer = &m_uniform_buffer});
    
    auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_GLOBAL);
    descriptor_set_globals->AddDescriptor<renderer::ImageSamplerDescriptor>(25)
        ->AddSubDescriptor({
            .image_view = &m_voxel_image->GetImageView(),
            .sampler    = &m_voxel_image->GetSampler()
        });
}

} // namespace hyperion::v2
