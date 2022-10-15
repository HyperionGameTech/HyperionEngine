#include <rendering/rt/RTRadianceRenderer.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Extent3D;
using renderer::StorageImageDescriptor;
using renderer::ResourceState;

Result RTRadianceRenderer::ImageOutput::Create(Device *device)
{
    HYPERION_BUBBLE_ERRORS(image.Create(device));
    HYPERION_BUBBLE_ERRORS(image_view.Create(device, &image));

    HYPERION_RETURN_OK;
}

Result RTRadianceRenderer::ImageOutput::Destroy(Device *device)
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

RTRadianceRenderer::RTRadianceRenderer(const Extent2D &extent)
    : m_extent(extent),
      m_image_outputs {
          ImageOutput(StorageImage(
              Extent3D(extent),
              Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
              Image::Type::TEXTURE_TYPE_2D
          )),
          ImageOutput(StorageImage(
              Extent3D(extent),
              Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
              Image::Type::TEXTURE_TYPE_2D
          )),
          ImageOutput(StorageImage(
              Extent3D(extent),
              Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R32F,
              Image::Type::TEXTURE_TYPE_2D
          ))
      },
      m_blur_radiance(
          extent,
          &m_image_outputs[0].image,
          &m_image_outputs[0].image_view,
          &m_image_outputs[1].image_view,
          &m_image_outputs[2].image_view
      )
{
}

RTRadianceRenderer::~RTRadianceRenderer()
{
}

void RTRadianceRenderer::Create(Engine *engine)
{
    CreateImages(engine);
    CreateDescriptors(engine);
    CreateRaytracingPipeline(engine);
    CreateBlurRadiance(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void RTRadianceRenderer::Destroy(Engine *engine)
{
    m_blur_radiance.Destroy(engine);

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        auto result = Result::OK;

        for (auto &image_output : m_image_outputs) {
            HYPERION_PASS_ERRORS(
                image_output.Destroy(engine->GetDevice()),
                result
            );
        }

        HYPERION_PASS_ERRORS(
            m_raytracing_pipeline->Destroy(engine->GetDevice()),
            result
        );

        return result;
    });

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void RTRadianceRenderer::Render(
    Engine *engine,
    Frame *frame
)
{
    m_raytracing_pipeline->Bind(frame->GetCommandBuffer());
    
    const auto scene_binding = engine->render_state.GetScene().id;
    const auto scene_index = scene_binding ? scene_binding.value - 1 : 0u;

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_raytracing_pipeline.Get(),
        DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()],
        DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
        FixedArray {
            UInt32(sizeof(v2::SceneShaderData) * scene_index),
            UInt32(sizeof(v2::LightDrawProxy) * 0)
        }
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_raytracing_pipeline.Get(),
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS, frame->GetFrameIndex()),
        DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_raytracing_pipeline.Get(),
        DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
        DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING
    );

    for (auto &image_output : m_image_outputs) {
        image_output.image.GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            ResourceState::UNORDERED_ACCESS
        );
    }

    m_raytracing_pipeline->TraceRays(
        engine->GetDevice(),
        frame->GetCommandBuffer(),
        m_image_outputs[0].image.GetExtent()
    );

    for (auto &image_output : m_image_outputs) {
        image_output.image.GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            ResourceState::UNORDERED_ACCESS
        );
    }

    m_blur_radiance.Render(engine, frame);
}

void RTRadianceRenderer::CreateImages(Engine *engine)
{
    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (auto &image_output : m_image_outputs) {
            HYPERION_BUBBLE_ERRORS(image_output.Create(engine->GetDevice()));
        }

        HYPERION_RETURN_OK;
    });
}

void RTRadianceRenderer::CreateDescriptors(Engine *engine)
{
    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        auto *rt_descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING);

        rt_descriptor_set->GetOrAddDescriptor<StorageImageDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[0].image_view
            });

        rt_descriptor_set->GetOrAddDescriptor<StorageImageDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[1].image_view
            });

        rt_descriptor_set->GetOrAddDescriptor<StorageImageDescriptor>(3)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[2].image_view
            });

        HYPERION_RETURN_OK;
    });
}

void RTRadianceRenderer::CreateRaytracingPipeline(Engine *engine)
{
    auto shader = std::make_unique<ShaderProgram>();

    shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_GEN, {
        FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/rt/test.rgen.spv")).Read()
    });

    shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_MISS, {
        FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/rt/test.rmiss.spv")).Read()
    });

    shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_CLOSEST_HIT, {
        FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/rt/test.rchit.spv")).Read()
    });

    m_raytracing_pipeline.Reset(new RaytracingPipeline(std::move(shader)));

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        return m_raytracing_pipeline->Create(engine->GetDevice(), &engine->GetInstance()->GetDescriptorPool());
    });
}

void RTRadianceRenderer::CreateBlurRadiance(Engine *engine)
{
    m_blur_radiance.Create(engine);
}


} // namespace hyperion::v2