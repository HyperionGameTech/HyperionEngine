#include "Voxelizer.hpp"
#include "../Engine.hpp"

#include <rendering/backend/RendererFeatures.hpp>

#include <math/MathUtil.hpp>
#include <asset/ByteReader.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::Result;

Voxelizer::Voxelizer()
    : EngineComponentBase(),
      m_num_fragments(0)
{
}

Voxelizer::~Voxelizer()
{
    Teardown();
}

void Voxelizer::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    const auto voxel_map_size_signed = static_cast<Int64>(voxel_map_size);

    m_scene = engine->CreateHandle<Scene>(
        engine->CreateHandle<Camera>(new OrthoCamera(
            voxel_map_size, voxel_map_size,
            -voxel_map_size_signed / 2, voxel_map_size_signed / 2,
            -voxel_map_size_signed / 2, voxel_map_size_signed / 2,
            -voxel_map_size_signed / 2, voxel_map_size_signed / 2
        ))
    );

    engine->InitObject(m_scene);
    
    CreateBuffers(engine);
    CreateShader(engine);
    CreateRenderPass(engine);
    CreateFramebuffer(engine);
    CreateDescriptors(engine);
    CreatePipeline(engine);

    OnTeardown([this]() {
        auto *engine = GetEngine();

        m_shader.Reset();
        m_framebuffer.Reset();
        m_render_pass.Reset();

        struct RENDER_COMMAND(DestroyVoxelizer) : RenderCommandBase2
        {
            Voxelizer &voxelizer;

            RENDER_COMMAND(DestroyVoxelizer)(Voxelizer &voxelizer)
                : voxelizer(voxelizer)
            {
            }

            virtual Result operator()(Engine *engine)
            {
                auto result = renderer::Result::OK;

                if (voxelizer.m_counter != nullptr) {
                    voxelizer.m_counter->Destroy(engine);
                }

                if (voxelizer.m_fragment_list_buffer != nullptr) {
                    engine->SafeRelease(UniquePtr<StorageBuffer>(voxelizer.m_fragment_list_buffer.release()));
                }

                for (auto &attachment : voxelizer.m_attachments) {
                    HYPERION_PASS_ERRORS(
                        attachment->Destroy(engine->GetInstance()->GetDevice()),
                        result
                    );
                }

                voxelizer.m_num_fragments = 0;

                return result;
            }
        };

        RenderCommands::Push<RENDER_COMMAND(DestroyVoxelizer)>(*this);

        HYP_FLUSH_RENDER_QUEUE(engine);

        m_renderer_instance.Reset();
        m_attachments.clear();

        m_fragment_list_buffer.reset();
        m_counter.reset();
    });
}

void Voxelizer::CreatePipeline(Engine *engine)
{
    auto renderer_instance = std::make_unique<RendererInstance>(
        std::move(m_shader),
        Handle<RenderPass>(m_render_pass),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
            },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::NONE,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
            }
        )
    );
    
    engine->InitObject(m_framebuffer);
    
    m_renderer_instance = engine->AddRendererInstance(std::move(renderer_instance));
    
    for (auto &item : engine->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
        for (auto &entity : item->GetEntities()) {
            if (entity != nullptr) {
                m_renderer_instance->AddEntity(Handle<Entity>(entity));
            }
        }
    }

    engine->InitObject(m_renderer_instance);
}

void Voxelizer::CreateBuffers(Engine *engine)
{
    m_counter = std::make_unique<AtomicCounter>();
    m_fragment_list_buffer = std::make_unique<StorageBuffer>();

    struct RENDER_COMMAND(CreateVoxelizerBuffers) : RenderCommandBase2
    {
        Voxelizer &voxelizer;

        RENDER_COMMAND(CreateVoxelizerBuffers)(Voxelizer &voxelizer)
            : voxelizer(voxelizer)
        {
        }

        virtual Result operator()(Engine *engine)
        {
            auto result = Result::OK;

            voxelizer.m_counter->Create(engine);
            
            HYPERION_PASS_ERRORS(
                voxelizer.m_fragment_list_buffer->Create(engine->GetInstance()->GetDevice(), default_fragment_list_buffer_size),
                result
            );

            return result;
        }
    };

    RenderCommands::Push<RENDER_COMMAND(CreateVoxelizerBuffers)>(*this);
}

void Voxelizer::CreateShader(Engine *engine)
{
    std::vector<SubShader> sub_shaders = {
        {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "/vkshaders/voxel/voxelize.vert.spv")).Read()}},
        {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "/vkshaders/voxel/voxelize.frag.spv")).Read()}}
    };

    if (engine->GetDevice()->GetFeatures().SupportsGeometryShaders()) {
        sub_shaders.push_back({ShaderModule::Type::GEOMETRY, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "/vkshaders/voxel/voxelize.geom.spv")).Read()}});
    } else {
        DebugLog(
            LogType::Debug,
            "Geometry shaders not supported on device, continuing without adding geometry shader to voxelizer pipeline.\n"
        );
    }
    
    m_shader = engine->CreateHandle<Shader>(sub_shaders);
    engine->InitObject(m_shader);
}

void Voxelizer::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->CreateHandle<RenderPass>(
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    engine->InitObject(m_render_pass);
}

void Voxelizer::CreateFramebuffer(Engine *engine)
{
    m_framebuffer = engine->CreateHandle<Framebuffer>(
        Extent2D { voxel_map_size, voxel_map_size },
        Handle<RenderPass>(m_render_pass)
    );
    
    engine->InitObject(m_framebuffer);
}

void Voxelizer::CreateDescriptors(Engine *engine)
{
    struct RENDER_COMMAND(CreateVoxelizerDescriptors) : RenderCommandBase2
    {
        Voxelizer &voxelizer;

        RENDER_COMMAND(CreateVoxelizerDescriptors)(Voxelizer &voxelizer)
            : voxelizer(voxelizer)
        {
        }

        virtual Result operator()(Engine *engine)
        {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

            descriptor_set
                ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(0)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .buffer = voxelizer.m_counter->GetBuffer()
                });

            descriptor_set
                ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(1)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .buffer = voxelizer.m_fragment_list_buffer.get()
                });

            HYPERION_RETURN_OK;
        }
    };

    RenderCommands::Push<RENDER_COMMAND(CreateVoxelizerDescriptors)>(*this);
}

/* We only reconstruct the buffer if the number of rendered fragments is
 * greater than what our buffer can hold (or the buffer has not yet been created).
 * We round up to the nearest power of two. */
void Voxelizer::ResizeFragmentListBuffer(Engine *engine, Frame *)
{
    const SizeType new_size = m_num_fragments * sizeof(Fragment);

    if (new_size <= m_fragment_list_buffer->size) {
        return;
    }

    //new_size = MathUtil::NextPowerOf2(new_size);

    DebugLog(
        LogType::Debug,
        "Resizing voxelizer fragment list buffer from %llu to %llu\n",
        m_fragment_list_buffer->size,
        new_size
    );

    engine->SafeRelease(UniquePtr<StorageBuffer>(m_fragment_list_buffer.release()));

    m_fragment_list_buffer.reset(new StorageBuffer);
    
    HYPERION_ASSERT_RESULT(m_fragment_list_buffer->Create(
        engine->GetInstance()->GetDevice(),
        new_size
    ));

    // TODO! frame index
    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

    descriptor_set->GetDescriptor(1)->RemoveSubDescriptor(0);
    descriptor_set->GetDescriptor(1)->SetSubDescriptor({
        .element_index = 0u,
        .buffer = m_fragment_list_buffer.get()
    });

    descriptor_set->ApplyUpdates(engine->GetInstance()->GetDevice());
}

void Voxelizer::RenderFragmentList(Engine *engine, Frame *, bool count_mode)
{
    m_renderer_instance->GetPipeline()->push_constants.voxelizer_data = {
        .grid_size = voxel_map_size,
        .count_mode = count_mode
    };

    auto single_time_commands = engine->GetInstance()->GetSingleTimeCommands();

    single_time_commands.Push([&](CommandBuffer *command_buffer) {
        auto temp_frame = Frame::TemporaryFrame(command_buffer);

        m_framebuffer->BeginCapture(command_buffer);
        
        if (!engine->render_state.GetScene()) {
            engine->render_state.BindScene(m_scene.Get());
            m_renderer_instance->Render(engine, &temp_frame);
            engine->render_state.UnbindScene();
        } else {
            m_renderer_instance->Render(engine, &temp_frame);
        }

        m_framebuffer->EndCapture(command_buffer);

        HYPERION_RETURN_OK;
    });

    HYPERION_ASSERT_RESULT(single_time_commands.Execute(engine->GetDevice()));
}

void Voxelizer::Render(Engine *engine, Frame *frame)
{
    m_scene->GetCamera()->UpdateMatrices();

    m_counter->Reset(engine);

    RenderFragmentList(engine, frame, true);

    m_num_fragments = m_counter->Read(engine);
    DebugLog(LogType::Debug, "Render %lu fragments (%llu MiB)\n", m_num_fragments, (m_num_fragments * sizeof(Fragment)) / 1000000ull);

    ResizeFragmentListBuffer(engine, frame);

    m_counter->Reset(engine);

    /* now we render the scene again, this time storing color values into the
     * fragment list buffer.  */
    RenderFragmentList(engine, frame, false);
}

} // namespace hyperion::v2
