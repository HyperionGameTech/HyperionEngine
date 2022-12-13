#include <rendering/EnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::Result;

const Extent3D EnvGrid::density = Extent3D { 4, 4, 2 };

const Extent2D EnvGrid::cubemap_dimensions = Extent2D { 256, 256 };

EnvGrid::EnvGrid(const BoundingBox &aabb)
    : RenderComponent(10),
      m_aabb(aabb),
      m_current_probe_index(0)
{
}

EnvGrid::~EnvGrid()
{
}

void EnvGrid::Init()
{
    Handle<Scene> scene(GetParent()->GetScene()->GetID());
    AssertThrow(scene.IsValid());

    const SizeType total_count = density.Size();
    const Vector3 aabb_extent = m_aabb.GetExtent();

    m_env_probes.Resize(total_count);

    for (SizeType x = 0; x < density.width; x++) {
        for (SizeType y = 0; y < density.height; y++) {
            for (SizeType z = 0; z < density.depth; z++) {
                const SizeType index = x * density.height * density.depth
                     + y * density.depth
                     + z;

                if (m_env_probes[index]) {
                    continue;
                }

                const BoundingBox env_probe_aabb(
                    m_aabb.min + (Vector3(Float(x), Float(y), Float(z)) * (aabb_extent / Vector3(density))),
                    m_aabb.min + (Vector3(Float(x + 1), Float(y + 1), Float(z + 1)) * (aabb_extent / Vector3(density)))
                );

                m_env_probes[index] = CreateObject<EnvProbe>(scene, m_aabb);
            }
        }
    }

    CreateShader();
    CreateFramebuffer();

    m_scene = CreateObject<Scene>(CreateObject<Camera>(
        90.0f,
        cubemap_dimensions.width, cubemap_dimensions.height,
        0.01f, (aabb_extent / Vector3(density)).Max()
    ));
    m_scene->GetCamera()->SetViewMatrix(Matrix4::LookAt(Vector3(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vector3(0.0f, 1.0f, 0.0f)));
    m_scene->GetCamera()->SetFramebuffer(m_framebuffer);
    m_scene->SetName("EnvGrid Scene");
    m_scene->SetOverrideRenderableAttributes(RenderableAttributeSet(
        MeshAttributes { },
        MaterialAttributes {
            .bucket = BUCKET_INTERNAL,
            .cull_faces = FaceCullMode::BACK
        },
        m_shader->GetID()
    ));
    m_scene->SetParentScene(Handle<Scene>(GetParent()->GetScene()->GetID()));

    InitObject(m_scene);
    Engine::Get()->GetWorld()->AddScene(m_scene);

    for (auto &env_probe : m_env_probes) {
        std::cout << "Init env probe at " << env_probe->GetAABB() << "\n";

        if (InitObject(env_probe)) {
            env_probe->EnqueueBind();
        }
    }

    std::cout << "Total probes: " << total_count << std::endl;
}

// called from game thread
void EnvGrid::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);
}

void EnvGrid::OnRemoved()
{
    for (auto &env_probe : m_env_probes) {
        env_probe->EnqueueUnbind();
        // GetParent()->GetScene()->RemoveEnvProbe(env_probe->GetID());
    }

    Engine::Get()->GetWorld()->RemoveScene(m_scene->GetID());
    m_scene.Reset();

    Engine::Get()->SafeReleaseHandle<Shader>(std::move(m_shader));

    // TODO: Destroy attachments
}

void EnvGrid::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    for (auto &env_probe : m_env_probes) {
        // TODO: Just update render data in Render()
        env_probe->Update();
    }
}

void EnvGrid::OnRender(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (m_env_probes.Empty()) {
        return;
    }

    const Vector3 &camera_position = Engine::Get()->GetRenderState().GetScene().camera.position;

    {
        EnvGridShaderData shader_data;
        shader_data.probe_indices[0] = m_current_probe_index; // TEMP
        shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
        shader_data.enabled_indices_mask = 1 << 0;

        Engine::Get()->GetRenderData()->env_grids.Set(GetComponentIndex(), shader_data);
    }

    auto *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    auto result = Result::OK;

    // TODO: Use paging controller for env probes to dynamically deallocate them etc.
    // only render in certain range.
    // Use EnqueueBind() and EnqueueUnbind() when loading in and unloading?

    struct alignas(128) { UInt32 env_probe_index; } push_constants;
    push_constants.env_probe_index = m_env_probes[m_current_probe_index]->GetID().ToIndex();
    m_scene->Render(frame, &push_constants, sizeof(push_constants));

    Image *framebuffer_image = m_framebuffer->GetAttachmentRefs()[0]->GetAttachment()->GetImage();

    Handle<Texture> &current_cubemap_texture = m_env_probes[m_current_probe_index]->GetTexture();

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
    current_cubemap_texture->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);
    current_cubemap_texture->GetImage().Blit(command_buffer, framebuffer_image);

    HYPERION_PASS_ERRORS(
        current_cubemap_texture->GetImage().GenerateMipmaps(Engine::Get()->GetGPUDevice(), command_buffer),
        result
    );

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    current_cubemap_texture->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    HYPERION_ASSERT_RESULT(result);

    m_current_probe_index = (m_current_probe_index + 1) % UInt(m_env_probes.Size());
}

void EnvGrid::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

void EnvGrid::CreateShader()
{
    m_shader = CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("CubemapRenderer"));
    InitObject(m_shader);
}

void EnvGrid::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        cubemap_dimensions,
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    AttachmentRef *attachment_ref;

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            cubemap_dimensions,
            InternalFormat::RGBA8_SRGB,
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    m_framebuffer->AddAttachmentRef(attachment_ref);

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            cubemap_dimensions,
            Engine::Get()->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    m_framebuffer->AddAttachmentRef(attachment_ref);

    // attachment should be created in render thread?
    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(Engine::Get()->GetGPUInstance()->GetDevice()));
    }

    InitObject(m_framebuffer);
}

} // namespace hyperion::v2
