#include "Voxelizer.hpp"
#include "../Engine.hpp"

#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Atomics.hpp>
#include <rendering/Framebuffer.hpp>
#include <scene/camera/Camera.hpp>

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

void Voxelizer::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    const auto voxel_map_size_signed = Int64(voxel_map_size);
    
    CreateBuffers();
    CreateShader();
    CreateFramebuffer();
    CreateDescriptors();

    m_camera = CreateObject<Camera>(voxel_map_size, voxel_map_size);
    m_camera->SetFramebuffer(m_framebuffer);

    m_camera->SetCameraController(UniquePtr<OrthoCameraController>::Construct(
        -voxel_map_size_signed / 2, voxel_map_size_signed / 2,
        -voxel_map_size_signed / 2, voxel_map_size_signed / 2,
        -voxel_map_size_signed / 2, voxel_map_size_signed / 2
    ));

    InitObject(m_camera);

    m_render_list.SetCamera(m_camera);

    OnTeardown([this]() {
        m_camera.Reset();
        m_render_list.Reset();

        m_shader.Reset();
        m_framebuffer.Reset();

        struct RENDER_COMMAND(DestroyVoxelizer) : renderer::RenderCommand
        {
            Voxelizer &voxelizer;

            RENDER_COMMAND(DestroyVoxelizer)(Voxelizer &voxelizer)
                : voxelizer(voxelizer)
            {
            }

            virtual Result operator()()
            {
                auto result = renderer::Result::OK;

                if (voxelizer.m_counter != nullptr) {
                    voxelizer.m_counter->Destroy();
                }

                if (voxelizer.m_fragment_list_buffer != nullptr) {
                    g_engine->SafeRelease(UniquePtr<StorageBuffer>(voxelizer.m_fragment_list_buffer.release()));
                }

                for (auto &attachment : voxelizer.m_attachments) {
                    HYPERION_PASS_ERRORS(
                        attachment->Destroy(g_engine->GetGPUInstance()->GetDevice()),
                        result
                    );
                }

                voxelizer.m_num_fragments = 0;

                return result;
            }
        };

        PUSH_RENDER_COMMAND(DestroyVoxelizer, *this);

        HYP_SYNC_RENDER();
        
        m_attachments.clear();

        m_fragment_list_buffer.reset();
        m_counter.reset();
    });
}

void Voxelizer::CreateBuffers()
{
    m_counter = std::make_unique<AtomicCounter>();
    m_fragment_list_buffer = std::make_unique<StorageBuffer>();

    struct RENDER_COMMAND(CreateVoxelizerBuffers) : renderer::RenderCommand
    {
        Voxelizer &voxelizer;

        RENDER_COMMAND(CreateVoxelizerBuffers)(Voxelizer &voxelizer)
            : voxelizer(voxelizer)
        {
        }

        virtual Result operator()()
        {
            auto result = Result::OK;

            voxelizer.m_counter->Create();
            
            HYPERION_PASS_ERRORS(
                voxelizer.m_fragment_list_buffer->Create(g_engine->GetGPUInstance()->GetDevice(), default_fragment_list_buffer_size),
                result
            );

            return result;
        }
    };

    PUSH_RENDER_COMMAND(CreateVoxelizerBuffers, *this);
}

void Voxelizer::CreateShader()
{
    Name shader_name = HYP_NAME(VCTVoxelizeWithGeometryShader);

    if (!g_engine->GetGPUDevice()->GetFeatures().SupportsGeometryShaders()) {
        shader_name = HYP_NAME(VCTVoxelizeWithoutGeometryShader);
    }

    m_shader = g_shader_manager->GetOrCreate(
        shader_name,
        ShaderProperties(renderer::static_mesh_vertex_attributes, { "MODE_SVO" })
    );

    AssertThrow(InitObject(m_shader));
}

void Voxelizer::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        Extent2D { voxel_map_size, voxel_map_size },
        renderer::RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );
    
    InitObject(m_framebuffer);
}

void Voxelizer::CreateDescriptors()
{
    struct RENDER_COMMAND(CreateVoxelizerDescriptors) : renderer::RenderCommand
    {
        Voxelizer &voxelizer;

        RENDER_COMMAND(CreateVoxelizerDescriptors)(Voxelizer &voxelizer)
            : voxelizer(voxelizer)
        {
        }

        virtual Result operator()()
        {
            DescriptorSetRef descriptor_set = g_engine->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL);

            descriptor_set
                ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::VCT_SVO_BUFFER)
                ->SetElementBuffer(0, voxelizer.m_counter->GetBuffer());

            descriptor_set
                ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::VCT_SVO_FRAGMENT_LIST)
                ->SetElementBuffer(0, voxelizer.m_fragment_list_buffer.get());

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(CreateVoxelizerDescriptors, *this);
}

/* We only reconstruct the buffer if the number of rendered fragments is
 * greater than what our buffer can hold (or the buffer has not yet been created).
 * We round up to the nearest power of two. */
void Voxelizer::ResizeFragmentListBuffer(Frame *frame)
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

    g_engine->SafeRelease(UniquePtr<StorageBuffer>(m_fragment_list_buffer.release()));

    m_fragment_list_buffer.reset(new StorageBuffer());
    
    HYPERION_ASSERT_RESULT(m_fragment_list_buffer->Create(
        g_engine->GetGPUInstance()->GetDevice(),
        new_size
    ));

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        DescriptorSetRef descriptor_set = g_engine->GetGPUInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

        descriptor_set
            ->GetDescriptor(DescriptorKey::VCT_SVO_FRAGMENT_LIST)
            ->SetElementBuffer(0, m_fragment_list_buffer.get());

        if (frame_index == frame->GetFrameIndex()) {
            descriptor_set->ApplyUpdates(g_engine->GetGPUInstance()->GetDevice());
        }
    }
}

void Voxelizer::RenderFragmentList(Frame *frame, const Scene *scene, bool count_mode)
{
    AssertThrow(scene != nullptr);

    auto single_time_commands = g_engine->GetGPUInstance()->GetSingleTimeCommands();

    single_time_commands.Push([&](const CommandBufferRef &command_buffer) {
        auto temp_frame = Frame::TemporaryFrame(command_buffer);

        struct alignas(128) {
            UInt32 grid_size;
            UInt32 count_mode;
        } push_constants;

        push_constants.grid_size = voxel_map_size;
        push_constants.count_mode = UInt32(count_mode);
        
        g_engine->GetRenderState().BindScene(scene);
        g_engine->GetRenderState().BindCamera(m_camera.Get());

        m_render_list.CollectDrawCalls(
            &temp_frame,
            Bitset((1 << BUCKET_OPAQUE)),
            nullptr
        );

        m_render_list.ExecuteDrawCalls(
            &temp_frame,
            Bitset(1 << BUCKET_OPAQUE),
            nullptr,
            &push_constants
        );

        g_engine->GetRenderState().UnbindCamera();
        g_engine->GetRenderState().UnbindScene();

        HYPERION_RETURN_OK;
    });

    HYPERION_ASSERT_RESULT(single_time_commands.Execute(g_engine->GetGPUDevice()));
}

void Voxelizer::CollectEntities(const Scene *scene)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(scene != nullptr);
    AssertThrow(m_camera.IsValid());
    AssertThrow(m_shader.IsValid());

    scene->CollectEntities(
        m_render_list,
        m_camera,
        Bitset((1 << BUCKET_OPAQUE)),
        RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .bucket     = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::NONE,
                .flags      = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
            },
            m_shader->GetCompiledShader().GetDefinition()
        ),
        true // skip culling
    );

    m_render_list.UpdateRenderGroups();
}

void Voxelizer::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_camera.IsValid());

    m_camera->Update(delta);
}

void Voxelizer::Render(Frame *frame, const Scene *scene)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_counter->Reset();

    RenderFragmentList(frame, scene, true);

    m_num_fragments = m_counter->Read();
    DebugLog(LogType::Debug, "Render %lu fragments (%llu MiB)\n", m_num_fragments, (m_num_fragments * sizeof(Fragment)) / 1000000ull);

    ResizeFragmentListBuffer(frame);

    m_counter->Reset();

    /* now we render the scene again, this time storing color values into the
     * fragment list buffer.  */
    RenderFragmentList(frame, scene, false);
}

} // namespace hyperion::v2
