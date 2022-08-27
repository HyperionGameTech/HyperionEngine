#include "Voxelizer.hpp"
#include "../Engine.hpp"

#include <math/MathUtil.hpp>
#include <asset/ByteReader.hpp>
#include <camera/OrthoCamera.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

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

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_VOXELIZER, [this](...) {
        auto *engine = GetEngine();

        const auto voxel_map_size_signed = static_cast<Int64>(voxel_map_size);

        m_scene = engine->CreateHandle<Scene>(
            engine->CreateHandle<Camera>(new OrthoCamera(
                voxel_map_size, voxel_map_size,
                -voxel_map_size_signed, voxel_map_size_signed,
                -voxel_map_size_signed, voxel_map_size_signed,
                -voxel_map_size_signed, voxel_map_size_signed
            ))
        );

        engine->InitObject(m_scene);

        if (m_counter == nullptr) {
            m_counter = std::make_unique<AtomicCounter>();
            m_counter->Create(engine);
        }

        if (m_fragment_list_buffer == nullptr) {
            m_fragment_list_buffer = std::make_unique<StorageBuffer>();

            HYPERION_ASSERT_RESULT(m_fragment_list_buffer->Create(engine->GetInstance()->GetDevice(), default_fragment_list_buffer_size));
        }
        
        CreateShader(engine);
        CreateRenderPass(engine);
        CreateFramebuffer(engine);
        CreateDescriptors(engine);
        CreatePipeline(engine);

        OnTeardown([this]() {
            auto *engine = GetEngine();

            auto result = renderer::Result::OK;

            if (m_counter != nullptr) {
                m_counter->Destroy(engine);
                m_counter.reset();
            }

            if (m_fragment_list_buffer != nullptr) {
                HYPERION_PASS_ERRORS(
                    m_fragment_list_buffer->Destroy(engine->GetInstance()->GetDevice()),
                    result
                );

                m_fragment_list_buffer.reset();
            }

            m_shader.Reset();
            m_framebuffer.Reset();
            m_render_pass.Reset();

            for (auto &attachment : m_attachments) {
                HYPERION_PASS_ERRORS(
                    attachment->Destroy(engine->GetInstance()->GetDevice()),
                    result
                );
            }

            m_attachments.clear();

            m_renderer_instance.Reset();

            m_num_fragments = 0;

            HYPERION_ASSERT_RESULT(result);
        });
    }));
}

void Voxelizer::CreatePipeline(Engine *engine)
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
    
    engine->InitObject(m_framebuffer);
    
    m_renderer_instance = engine->AddRendererInstance(std::move(renderer_instance));
    
    for (auto &item : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
        for (auto &entity : item->GetEntities()) {
            if (entity != nullptr) {
                m_renderer_instance->AddEntity(Handle<Entity>(entity));
            }
        }
    }

    engine->InitObject(m_renderer_instance);
}

void Voxelizer::CreateShader(Engine *engine)
{
    m_shader = engine->CreateHandle<Shader>(
        std::vector<SubShader>{
            {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/voxel/voxelize.vert.spv")).Read()}},
            {ShaderModule::Type::GEOMETRY, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/voxel/voxelize.geom.spv")).Read()}},
            {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/voxel/voxelize.frag.spv")).Read()}}
        }
    );

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
    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

    descriptor_set
        ->AddDescriptor<renderer::StorageBufferDescriptor>(0)
        ->SetSubDescriptor({.buffer = m_counter->GetBuffer()});

    descriptor_set
        ->AddDescriptor<renderer::StorageBufferDescriptor>(1)
        ->SetSubDescriptor({.buffer = m_fragment_list_buffer.get()});
}

/* We only reconstruct the buffer if the number of rendered fragments is
 * greater than what our buffer can hold (or the buffer has not yet been created).
 * We round up to the nearest power of two. */
void Voxelizer::ResizeFragmentListBuffer(Engine *engine)
{
    const size_t new_size = m_num_fragments * sizeof(Fragment);

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

    HYPERION_ASSERT_RESULT(m_fragment_list_buffer->Destroy(
        engine->GetInstance()->GetDevice()
    ));

    m_fragment_list_buffer.reset(new StorageBuffer);
    
    HYPERION_ASSERT_RESULT(m_fragment_list_buffer->Create(
        engine->GetInstance()->GetDevice(),
        new_size
    ));

    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

    descriptor_set->GetDescriptor(1)->RemoveSubDescriptor(0);
    descriptor_set->GetDescriptor(1)->SetSubDescriptor({.buffer = m_fragment_list_buffer.get()});

    descriptor_set->ApplyUpdates(engine->GetInstance()->GetDevice());
}

void Voxelizer::RenderFragmentList(Engine *engine, bool count_mode)
{
    auto commands = engine->GetInstance()->GetSingleTimeCommands();

    /* count number of fragments */
    commands.Push([this, engine, count_mode](CommandBuffer *command_buffer) {
        Frame frame = Frame::TemporaryFrame(command_buffer, 0);

        engine->render_state.BindScene(m_scene.Get());

        m_renderer_instance->GetPipeline()->push_constants.voxelizer_data = {
            .grid_size = voxel_map_size,
            .count_mode = count_mode
        };

        m_framebuffer->BeginCapture(command_buffer);
        m_renderer_instance->Render(engine, &frame);
        m_framebuffer->EndCapture(command_buffer);

        engine->render_state.UnbindScene();

        HYPERION_RETURN_OK;
    });

    HYPERION_ASSERT_RESULT(commands.Execute(engine->GetInstance()->GetDevice()));
}

void Voxelizer::Render(Engine *engine)
{
    m_scene->GetCamera()->UpdateMatrices();

    m_counter->Reset(engine);

    RenderFragmentList(engine, true);

    m_num_fragments = m_counter->Read(engine);
    DebugLog(LogType::Debug, "Render %lu fragments (%llu MiB)\n", m_num_fragments, (m_num_fragments * sizeof(Fragment)) / 1000000ull);

    ResizeFragmentListBuffer(engine);

    m_counter->Reset(engine);

    /* now we render the scene again, this time storing color values into the
     * fragment list buffer.  */
    RenderFragmentList(engine, false);
}

} // namespace hyperion::v2
