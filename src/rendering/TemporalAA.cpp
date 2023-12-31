#include <rendering/TemporalAA.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <math/Vector2.hpp>
#include <math/MathUtil.hpp>
#include <math/Halton.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::UniformBufferDescriptor;
using renderer::DynamicStorageBufferDescriptor;
using renderer::ShaderVec2;
using renderer::ShaderMat4;

struct RENDER_COMMAND(SetTemporalAAResultInGlobalDescriptorSet) : renderer::RenderCommand
{
    Handle<Texture> result_texture;

    RENDER_COMMAND(SetTemporalAAResultInGlobalDescriptorSet)(
        Handle<Texture> result_texture
    ) : result_texture(std::move(result_texture))
    {
    }

    virtual Result operator()()
    {
        ImageView *result_texture_view = result_texture.IsValid()
            ? result_texture->GetImageView()
            : &g_engine->GetPlaceholderData().GetImageView2D1x1R8();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // Add the final result to the global descriptor set
            DescriptorSetRef descriptor_set_globals = g_engine->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::TEMPORAL_AA_RESULT)
                ->SetElementSRV(0, result_texture_view);
        }

        HYPERION_RETURN_OK;
    }
};

Result TemporalAA::ImageOutput::Create(Device *device)
{
    HYPERION_BUBBLE_ERRORS(image.Create(device));
    HYPERION_BUBBLE_ERRORS(image_view.Create(device, &image));

    HYPERION_RETURN_OK;
}

Result TemporalAA::ImageOutput::Destroy(Device *device)
{
    auto result = Result::OK;

    HYPERION_PASS_ERRORS(
        image.Destroy(device),
        result
    );

    HYPERION_PASS_ERRORS(
        image_view.Destroy(device),
        result
    );

    return result;
}

TemporalAA::TemporalAA(const Extent2D &extent)
    : m_extent(extent)
{
}

TemporalAA::~TemporalAA() = default;

void TemporalAA::Create()
{
    CreateImages();
    CreateComputePipelines();
}

void TemporalAA::Destroy()
{
    m_compute_taa.Reset();

    // release our owned descriptor sets
    SafeRelease(std::move(m_descriptor_sets));
    SafeRelease(std::move(m_uniform_buffers));

    PUSH_RENDER_COMMAND(
        SetTemporalAAResultInGlobalDescriptorSet,
        Handle<Texture>::empty
    );

    g_safe_deleter->SafeReleaseHandle(std::move(m_result_texture));
    g_safe_deleter->SafeReleaseHandle(std::move(m_history_texture));
}

void TemporalAA::CreateImages()
{
    m_result_texture = CreateObject<Texture>(Texture2D(
        m_extent,
        InternalFormat::RGBA16F,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_result_texture->GetImage()->SetIsRWTexture(true);

    InitObject(m_result_texture);

    m_history_texture = CreateObject<Texture>(Texture2D(
        m_extent,
        InternalFormat::RGBA16F,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_history_texture->GetImage()->SetIsRWTexture(true);

    InitObject(m_history_texture);

    PUSH_RENDER_COMMAND(
        SetTemporalAAResultInGlobalDescriptorSet,
        m_result_texture
    );

    HYP_SYNC_RENDER();
}

void TemporalAA::CreateComputePipelines()
{
    auto shader = g_shader_manager->GetOrCreate(HYP_NAME(TemporalAA));

    const renderer::DescriptorTable descriptor_table = shader->GetCompiledShader().GetDefinition().GetDescriptorUsages().BuildDescriptorTable();

    const renderer::DescriptorSetDeclaration *descriptor_set_decl = descriptor_table.FindDescriptorSetDeclaration(HYP_NAME(TemporalAADescriptorSet));
    AssertThrow(descriptor_set_decl != nullptr);

    const FixedArray<Handle<Texture> *, 2> textures = {
        &m_result_texture,
        &m_history_texture
    };

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        // create descriptor sets for depth pyramid generation.
        DescriptorSetRef descriptor_set = MakeRenderObject<renderer::DescriptorSet>(*descriptor_set_decl);

        if (auto *descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(InColorTexture))) {
            descriptor->SetElementSRV(0, g_engine->GetDeferredRenderer().GetCombinedResult()->GetImageView());
        } else {
            AssertThrowMsg(false, "Could not find descriptor InColorTexture");
        }

        if (auto *descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(InPrevColorTexture))) {
            descriptor->SetElementSRV(0, (*textures[(frame_index + 1) % 2])->GetImageView());
        } else {
            AssertThrowMsg(false, "Could not find descriptor InPrevColorTexture");
        }

        if (auto *descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(InVelocityTexture))) {
            descriptor->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                .GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView());
        } else {
            AssertThrowMsg(false, "Could not find descriptor InVelocityTexture");
        }

        if (auto *descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(InDepthTexture))) {
            descriptor->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());
        } else {
            AssertThrowMsg(false, "Could not find descriptor InDepthTexture");
        }

        if (auto *descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(SamplerLinear))) {
            descriptor->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());
        } else {
            AssertThrowMsg(false, "Could not find descriptor SamplerLinear");
        }

        if (auto *descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(SamplerNearest))) {
            descriptor->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerNearest());
        } else {
            AssertThrowMsg(false, "Could not find descriptor SamplerNearest");
        }

        if (auto *descriptor = descriptor_set->GetDescriptorByName(HYP_NAME(OutColorImage))) {
            descriptor->SetElementUAV(0, (*textures[frame_index % 2])->GetImageView());
        } else {
            AssertThrowMsg(false, "Could not find descriptor OutColorImage");
        }

        DeferCreate(
            descriptor_set,
            g_engine->GetGPUDevice(),
            &g_engine->GetGPUInstance()->GetDescriptorPool()
        );

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    m_compute_taa = CreateObject<ComputePipeline>(
        shader,
        Array<DescriptorSetRef> { m_descriptor_sets[0] }
    );

    InitObject(m_compute_taa);
}

void TemporalAA::Render(Frame *frame)
{
    const auto &scene = g_engine->GetRenderState().GetScene().scene;
    const auto &camera = g_engine->GetRenderState().GetCamera().camera;

    const FixedArray<Handle<Texture> *, 2> textures {
        &m_result_texture,
        &m_history_texture
    };

    Handle<Texture> &active_texture = *textures[frame->GetFrameIndex() % 2];

    active_texture->GetImage()->GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    struct alignas(128) {
        ShaderVec2<UInt32>  dimensions;
        ShaderVec2<UInt32>  depth_texture_dimensions;
        ShaderVec2<Float>   camera_near_far;
    } push_constants;

    push_constants.dimensions = m_extent;
    push_constants.depth_texture_dimensions = Extent2D(
        g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetAttachment()->GetImage()->GetExtent()
    );
    push_constants.camera_near_far = Vector2(camera.clip_near, camera.clip_far);

    m_compute_taa->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
    m_compute_taa->GetPipeline()->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_compute_taa->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()],
        0
    );

    m_compute_taa->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (m_extent.width + 7) / 8,
            (m_extent.height + 7) / 8,
            1
        }
    );
    
    active_texture->GetImage()->GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion::v2