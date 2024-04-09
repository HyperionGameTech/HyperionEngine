#include "TemporalBlending.hpp"
#include <Engine.hpp>
#include <Threads.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::Rect;
using renderer::ShaderVec2;

#pragma region Render commands

struct RENDER_COMMAND(CreateTemporalBlendingImageOutputs) : renderer::RenderCommand
{
    TemporalBlending::ImageOutput *image_outputs;

    RENDER_COMMAND(CreateTemporalBlendingImageOutputs)(TemporalBlending::ImageOutput *image_outputs)
        : image_outputs(image_outputs)
    {
    }

    virtual ~RENDER_COMMAND(CreateTemporalBlendingImageOutputs)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(image_outputs[frame_index].Create(g_engine->GetGPUDevice()));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

TemporalBlending::TemporalBlending(
    const Extent2D &extent,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const FixedArray<ImageViewRef, max_frames_in_flight> &input_image_views
) : TemporalBlending(
        extent,
        InternalFormat::RGBA8,
        technique,
        feedback,
        input_image_views
    )
{
}

TemporalBlending::TemporalBlending(
    const Extent2D &extent,
    InternalFormat image_format,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const Handle<Framebuffer> &input_framebuffer
) : m_extent(extent),
    m_image_format(image_format),
    m_technique(technique),
    m_feedback(feedback),
    m_input_framebuffer(input_framebuffer),
    m_blending_frame_counter(0)
{
}

TemporalBlending::TemporalBlending(
    const Extent2D &extent,
    InternalFormat image_format,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const FixedArray<ImageViewRef, max_frames_in_flight> &input_image_views
) : m_extent(extent),
    m_image_format(image_format),
    m_technique(technique),
    m_feedback(feedback),
    m_input_image_views(input_image_views),
    m_blending_frame_counter(0)
{
}

TemporalBlending::~TemporalBlending() = default;

void TemporalBlending::Create()
{
    InitObject(m_input_framebuffer);

    CreateImageOutputs();
    CreateDescriptorSets();
    CreateComputePipelines();
}

void TemporalBlending::Destroy()
{
    m_perform_blending.Reset();

    SafeRelease(std::move(m_descriptor_table));

    for (auto &image_output : m_image_outputs) {
        SafeRelease(std::move(image_output.image));
        SafeRelease(std::move(image_output.image_view));
    }
}

ShaderProperties TemporalBlending::GetShaderProperties() const
{
    ShaderProperties shader_properties;

    switch (m_image_format) {
    case InternalFormat::RGBA8:
        shader_properties.Set("OUTPUT_RGBA8");
        break;
    case InternalFormat::RGBA16F:
        shader_properties.Set("OUTPUT_RGBA16F");
        break;
    case InternalFormat::RGBA32F:
        shader_properties.Set("OUTPUT_RGBA32F");
        break;
    default:
        AssertThrowMsg(false, "Unsupported format for temporal blending: %u\n", uint(m_image_format));

        break;
    }

    static const String feedback_strings[] = { "LOW", "MEDIUM", "HIGH" };

    shader_properties.Set("TEMPORAL_BLEND_TECHNIQUE_" + String::ToString(uint(m_technique)));
    shader_properties.Set("FEEDBACK_" + feedback_strings[MathUtil::Min(uint(m_feedback), ArraySize(feedback_strings) - 1)]);

    return shader_properties;
}

void TemporalBlending::CreateImageOutputs()
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto &image_output = m_image_outputs[frame_index];

        image_output.image = MakeRenderObject<Image>(StorageImage(
            Extent3D(m_extent),
            m_image_format,
            ImageType::TEXTURE_TYPE_2D,
            FilterMode::TEXTURE_FILTER_LINEAR,
            FilterMode::TEXTURE_FILTER_LINEAR,
            nullptr
        ));

        image_output.image_view = MakeRenderObject<ImageView>();
    }

    PUSH_RENDER_COMMAND(CreateTemporalBlendingImageOutputs, m_image_outputs.Data());
}

void TemporalBlending::CreateDescriptorSets()
{
    Handle<Shader> shader = g_shader_manager->GetOrCreate(HYP_NAME(TemporalBlending), GetShaderProperties());
    AssertThrow(shader.IsValid());

    const renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

    m_descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        if (m_input_framebuffer) {
            AssertThrowMsg(m_input_framebuffer->GetAttachmentUsages().Size() != 0, "No attachment refs on input framebuffer!");
        }

        const ImageViewRef input_image_view = m_input_framebuffer ? m_input_framebuffer->GetAttachmentUsages()[0]->GetImageView() : m_input_image_views[frame_index];
        AssertThrow(input_image_view != nullptr);

        // input image
        m_descriptor_table->GetDescriptorSet(HYP_NAME(TemporalBlendingDescriptorSet), frame_index)
            ->SetElement(HYP_NAME(InImage), input_image_view);

        m_descriptor_table->GetDescriptorSet(HYP_NAME(TemporalBlendingDescriptorSet), frame_index)
            ->SetElement(HYP_NAME(PrevImage), m_image_outputs[(frame_index + 1) % max_frames_in_flight].image_view);

        m_descriptor_table->GetDescriptorSet(HYP_NAME(TemporalBlendingDescriptorSet), frame_index)
            ->SetElement(HYP_NAME(VelocityImage), g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView());

        m_descriptor_table->GetDescriptorSet(HYP_NAME(TemporalBlendingDescriptorSet), frame_index)
            ->SetElement(HYP_NAME(SamplerLinear), g_engine->GetPlaceholderData()->GetSamplerLinear());

        m_descriptor_table->GetDescriptorSet(HYP_NAME(TemporalBlendingDescriptorSet), frame_index)
            ->SetElement(HYP_NAME(SamplerNearest), g_engine->GetPlaceholderData()->GetSamplerNearest());

        m_descriptor_table->GetDescriptorSet(HYP_NAME(TemporalBlendingDescriptorSet), frame_index)
            ->SetElement(HYP_NAME(OutImage), m_image_outputs[frame_index].image_view);
    }

    DeferCreate(
        m_descriptor_table,
        g_engine->GetGPUDevice()
    );
}

void TemporalBlending::CreateComputePipelines()
{
    AssertThrow(m_descriptor_table.IsValid());

    Handle<Shader> shader = g_shader_manager->GetOrCreate(HYP_NAME(TemporalBlending), GetShaderProperties());
    AssertThrow(shader.IsValid());

    m_perform_blending = MakeRenderObject<renderer::ComputePipeline>(
        shader->GetShaderProgram(),
        m_descriptor_table
    );

    DeferCreate(
        m_perform_blending,
        g_engine->GetGPUDevice()
    );
}

void TemporalBlending::Render(Frame *frame)
{   
    m_image_outputs[frame->GetFrameIndex()].image->GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    const Extent3D &extent = m_image_outputs[frame->GetFrameIndex()].image->GetExtent();

    struct alignas(128) {
        ShaderVec2<uint32>  output_dimensions;
        ShaderVec2<uint32>  depth_texture_dimensions;
        uint32              blending_frame_counter;
    } push_constants;

    push_constants.output_dimensions = Extent2D(extent);
    push_constants.depth_texture_dimensions = Extent2D(
        g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetAttachment()->GetImage()->GetExtent()
    );

    push_constants.blending_frame_counter = m_blending_frame_counter;

    m_perform_blending->SetPushConstants(&push_constants, sizeof(push_constants));
    m_perform_blending->Bind(frame->GetCommandBuffer());

    m_descriptor_table->Bind(
        frame,
        m_perform_blending,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                }
            }
        }
    );

    m_perform_blending->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (extent.width + 7) / 8,
            (extent.height + 7) / 8,
            1
        }
    );

    // set it to be able to be used as texture2D for next pass, or outside of this
    m_image_outputs[frame->GetFrameIndex()].image->GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);

    m_blending_frame_counter = m_technique == TemporalBlendTechnique::TECHNIQUE_4
        ? m_blending_frame_counter + 1
        : 0;
}

} // namespace hyperion::v2
