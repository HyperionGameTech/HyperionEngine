#include <rendering/EnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <scene/controllers/PagingController.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::Result;

class EnvGrid;

class EnvGridPagingController : public PagingController
{
public:
    EnvGridPagingController(
        EnvGrid *env_grid,
        Extent3D patch_size,
        const Vector3 &scale,
        Float max_distance
    );
    virtual ~EnvGridPagingController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;

protected:
    virtual void OnPatchAdded(Patch *patch) override;
    virtual void OnPatchRemoved(Patch *patch) override;

private:
    EnvGrid *m_env_grid;
};

EnvGridPagingController::EnvGridPagingController(
    EnvGrid *env_grid,
    Extent3D patch_size,
    const Vector3 &scale,
    Float max_distance
) : PagingController("EnvGridPagingController", patch_size, scale, max_distance),
    m_env_grid(env_grid)
{
}

void EnvGridPagingController::OnAdded()
{
    AssertThrow(m_env_grid != nullptr);

    PagingController::OnAdded();
}

void EnvGridPagingController::OnRemoved()
{
    PagingController::OnRemoved();
}

void EnvGridPagingController::OnPatchAdded(Patch *patch)
{
    DebugLog(LogType::Info, "EnvGrid added %f, %f\n", patch->info.coord.x, patch->info.coord.y);
}

void EnvGridPagingController::OnPatchRemoved(Patch *patch)
{
    DebugLog(LogType::Info, "EnvGrid removed %f, %f\n", patch->info.coord.x, patch->info.coord.y);
}


const Extent3D EnvGrid::density = Extent3D { 2, 2, 2 };
const Extent2D EnvGrid::cubemap_dimensions = Extent2D { 128, 128 };

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

                m_env_probes[index] = CreateObject<EnvProbe>(scene, env_probe_aabb);
            }
        }
    }

    CreateShader();
    CreateFramebuffer();

    m_scene = CreateObject<Scene>(CreateObject<Camera>(
        90.0f,
        cubemap_dimensions.width, cubemap_dimensions.height,
        0.01f, (aabb_extent / Vector3(density)).Max() * 0.5f
    ));
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
        if (InitObject(env_probe)) {
            env_probe->EnqueueBind();
        }
    }

    DebugLog(LogType::Info, "Created %llu total EnvProbes in grid\n", total_count);
}

// called from game thread
void EnvGrid::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity = CreateObject<Entity>();
    m_entity->SetName("EnvGrid Entity");
    m_entity->AddController<EnvGridPagingController>(
        this,
        Extent3D(m_aabb.GetExtent() / Vector3(density)),
        Vector3::one,
        1.0f
    );

    GetParent()->GetScene()->AddEntity(m_entity);
}

void EnvGrid::OnRemoved()
{
    if (m_entity) {
        m_entity->RemoveController<EnvGridPagingController>();

        GetParent()->GetScene()->RemoveEntity(m_entity);
        m_entity.Reset();
    }

    for (auto &env_probe : m_env_probes) {
        env_probe->EnqueueUnbind();
    }

    Engine::Get()->GetWorld()->RemoveScene(m_scene->GetID());
    m_scene.Reset();

    Engine::Get()->SafeReleaseHandle<Shader>(std::move(m_shader));

    struct RENDER_COMMAND(DestroyEnvGridFramebufferAttachments) : RenderCommand
    {
        EnvGrid &env_grid;

        RENDER_COMMAND(DestroyEnvGridFramebufferAttachments)(EnvGrid &env_grid)
            : env_grid(env_grid)
        {
        }

        virtual Result operator()()
        {
            auto result = Result::OK;

            if (env_grid.m_framebuffer != nullptr) {
                for (auto &attachment : env_grid.m_attachments) {
                    env_grid.m_framebuffer->RemoveAttachmentRef(attachment.get());
                }
            }

            for (auto &attachment : env_grid.m_attachments) {
                HYPERION_PASS_ERRORS(attachment->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()), result);
            }

            env_grid.m_attachments.clear();

            return result;
        }
    };

    PUSH_RENDER_COMMAND(DestroyEnvGridFramebufferAttachments, *this);
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

    const UInt num_env_probes = UInt(m_env_probes.Size());

    // TEMP TESTING! remove.
    const Vector3 &camera_position = Engine::Get()->GetRenderState().GetScene().scene.camera.position;

    {
        UInt idx = 0;
        for (auto &it : m_env_probes) {
            if (it->GetAABB().ContainsPoint(camera_position)) {
                m_current_probe_index = idx;
                break;
            }

            ++idx;
        }
    }


    Handle<EnvProbe> &current_env_probe = m_env_probes[m_current_probe_index];

    const UInt bound_index = 0; // TEMP: Just for testing.
    current_env_probe->UpdateRenderData(bound_index);

    {
        EnvGridShaderData shader_data;
        shader_data.probe_indices[0] = current_env_probe->GetID().ToIndex();//(m_current_probe_index + num_env_probes - 1) % num_env_probes; // TEMP
        shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
        shader_data.enabled_indices_mask = 1 << 0;

        DebugLog(LogType::Debug, " probe indices[0] = %u\n", shader_data.probe_indices[0]);

        Engine::Get()->GetRenderData()->env_grids.Set(GetComponentIndex(), shader_data);
    }

    auto *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    auto result = Result::OK;

    // Use EnqueueBind() and EnqueueUnbind() when loading in and unloading?

    struct alignas(128) { UInt32 env_probe_index; } push_constants;
    push_constants.env_probe_index = current_env_probe->GetID().ToIndex();
    m_scene->Render(frame, &push_constants, sizeof(push_constants));

    { // copy current framebuffer image to EnvProbe texture, generate mipmap
        Image *framebuffer_image = m_framebuffer->GetAttachmentRefs()[0]->GetAttachment()->GetImage();
        Handle<Texture> &current_cubemap_texture = current_env_probe->GetTexture();

        framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
        current_cubemap_texture->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        HYPERION_PASS_ERRORS(
            current_cubemap_texture->GetImage().Blit(command_buffer, framebuffer_image),
            result
        );

        HYPERION_PASS_ERRORS(
            current_cubemap_texture->GetImage().GenerateMipmaps(Engine::Get()->GetGPUDevice(), command_buffer),
            result
        );

        framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        current_cubemap_texture->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

        HYPERION_ASSERT_RESULT(result);
    }

    m_current_probe_index = (m_current_probe_index + 1) % num_env_probes;
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
