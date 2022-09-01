#include "ProbeSystem.hpp"
#include "../Engine.hpp"
#include <Types.hpp>

#include <rendering/backend/RendererShader.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::ShaderProgram;
using renderer::StorageImageDescriptor;
using renderer::StorageBufferDescriptor;
using renderer::UniformBufferDescriptor;
using renderer::GPUMemory;

ProbeGrid::ProbeGrid(ProbeGridInfo &&grid_info)
    : m_grid_info(std::move(grid_info)),
      m_time(0)
{
}

ProbeGrid::~ProbeGrid()
{
}

void ProbeGrid::Init(Engine *engine)
{
    const auto grid = m_grid_info.NumProbesPerDimension();
    
    m_probes.resize(m_grid_info.NumProbes());

    for (UInt32 x = 0; x < grid.width; x++) {
        for (UInt32 y = 0; y < grid.height; y++) {
            for (UInt32 z = 0; z < grid.depth; z++) {
                const UInt32 index = x * grid.height * grid.depth
                                     + y * grid.depth
                                     + z;

                m_probes[index] = Probe{
                    .position = (Vector3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) - (Vector(m_grid_info.probe_border) * 0.5f)) * m_grid_info.probe_distance
                };
            }
        }
    }

    CreateStorageBuffers(engine);
    CreateUniformBuffer(engine);
    AddDescriptors(engine);

    engine->callbacks.Once(EngineCallback::CREATE_RAYTRACING_PIPELINES, [this](Engine *engine) {
        CreatePipeline(engine);
    });
    
    CreateComputePipelines(engine);
}

void ProbeGrid::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;

    Device *device = engine->GetDevice();

    HYPERION_PASS_ERRORS(m_uniform_buffer->Destroy(device), result);
    
    HYPERION_PASS_ERRORS(m_radiance_buffer->Destroy(device), result);

    HYPERION_PASS_ERRORS(m_irradiance_image->Destroy(device), result);
    HYPERION_PASS_ERRORS(m_irradiance_image_view->Destroy(device), result);
    
    HYPERION_PASS_ERRORS(m_depth_image->Destroy(device), result);
    HYPERION_PASS_ERRORS(m_depth_image_view->Destroy(device), result);

    HYPERION_PASS_ERRORS(m_pipeline->Destroy(device), result);

    HYPERION_ASSERT_RESULT(result);
}

void ProbeGrid::CreatePipeline(Engine *engine)
{
    auto rt_shader = std::make_unique<ShaderProgram>();

    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_GEN, {
        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/rt/probe.rgen.spv")).Read()
    });
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_MISS, {
        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/rt/probe.rmiss.spv")).Read()
    });
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_CLOSEST_HIT, {
        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/rt/probe.rchit.spv")).Read()
    });

    m_pipeline = std::make_unique<RaytracingPipeline>(std::move(rt_shader));
    HYPERION_ASSERT_RESULT(m_pipeline->Create(engine->GetDevice(), &engine->GetInstance()->GetDescriptorPool()));
}

void ProbeGrid::CreateComputePipelines(Engine *engine)
{
    m_update_irradiance = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                {ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/probe_update_irradiance.comp.spv")).Read()}}
            }
        )
    );

    engine->InitObject(m_update_irradiance);

    m_update_depth = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                {ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/probe_update_depth.comp.spv")).Read()}}
            }
        )
    );

    engine->InitObject(m_update_depth);
}

void ProbeGrid::CreateUniformBuffer(Engine *engine)
{
    ProbeSystemUniforms uniforms{
        .aabb_max                     = m_grid_info.aabb.max.ToVector4(),
        .aabb_min                     = m_grid_info.aabb.min.ToVector4(),
        .probe_border                 = m_grid_info.probe_border,
        .probe_counts                 = m_grid_info.NumProbesPerDimension(),
        .image_dimensions             = m_grid_info.GetImageDimensions(),
        .irradiance_image_dimensions  = Extent2D(m_irradiance_image->GetExtent()),
        .depth_image_dimensions       = Extent2D(m_depth_image->GetExtent()),
        .probe_distance               = m_grid_info.probe_distance,
        .num_rays_per_probe           = m_grid_info.num_rays_per_probe
    };

    m_uniform_buffer = std::make_unique<UniformBuffer>();
    HYPERION_ASSERT_RESULT(m_uniform_buffer->Create(engine->GetDevice(), sizeof(ProbeSystemUniforms)));

    m_uniform_buffer->Copy(engine->GetDevice(), sizeof(ProbeSystemUniforms), &uniforms);
}

void ProbeGrid::CreateStorageBuffers(Engine *engine)
{
    const auto probe_counts = m_grid_info.NumProbesPerDimension();

    m_radiance_buffer = std::make_unique<StorageBuffer>();
    HYPERION_ASSERT_RESULT(m_radiance_buffer->Create(engine->GetDevice(), m_grid_info.GetImageDimensions().Size() * sizeof(ProbeRayData)));

    m_irradiance_image = std::make_unique<StorageImage>(
        Extent3D{{(m_grid_info.irradiance_octahedron_size + 2) * probe_counts.width * probe_counts.height + 2, (m_grid_info.irradiance_octahedron_size + 2) * probe_counts.depth + 2}},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );
    HYPERION_ASSERT_RESULT(m_irradiance_image->Create(engine->GetDevice()));

    m_irradiance_image_view = std::make_unique<ImageView>();
    HYPERION_ASSERT_RESULT(m_irradiance_image_view->Create(engine->GetDevice(), m_irradiance_image.get()));

    m_depth_image = std::make_unique<StorageImage>(
        Extent3D{{(m_grid_info.depth_octahedron_size + 2) * probe_counts.width * probe_counts.height + 2, (m_grid_info.depth_octahedron_size + 2) * probe_counts.depth + 2}},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );
    HYPERION_ASSERT_RESULT(m_depth_image->Create(engine->GetDevice()));

    m_depth_image_view = std::make_unique<ImageView>();
    HYPERION_ASSERT_RESULT(m_depth_image_view->Create(engine->GetDevice(), m_depth_image.get()));
}

void ProbeGrid::AddDescriptors(Engine *engine)
{
    /* TMP, have to set this up in a way such that it's not relying on something else to populate the other RT stuff */

    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING);

    auto *probe_uniforms = descriptor_set->AddDescriptor<UniformBufferDescriptor>(9);
    probe_uniforms->SetSubDescriptor({.buffer = m_uniform_buffer.get()});

    auto *probe_ray_data = descriptor_set->AddDescriptor<StorageBufferDescriptor>(10);
    probe_ray_data->SetSubDescriptor({.buffer = m_radiance_buffer.get()});

    auto *irradiance_image_descriptor = descriptor_set->AddDescriptor<StorageImageDescriptor>(11);
    irradiance_image_descriptor->SetSubDescriptor({.image_view = m_irradiance_image_view.get()});

    auto *irradiance_depth_image_descriptor = descriptor_set->AddDescriptor<StorageImageDescriptor>(12);
    irradiance_depth_image_descriptor->SetSubDescriptor({.image_view = m_depth_image_view.get()});
}

void ProbeGrid::SubmitPushConstants(Engine *engine, CommandBuffer *command_buffer)
{
    m_random_generator.Next();

    std::memcpy(
        m_pipeline->push_constants.probe_data.matrix,
        m_random_generator.matrix.values,
        std::size(m_pipeline->push_constants.probe_data.matrix) * sizeof(m_pipeline->push_constants.probe_data.matrix[0])
    );

    m_pipeline->push_constants.probe_data.time = m_time++;

    m_pipeline->SubmitPushConstants(command_buffer);
}

void ProbeGrid::RenderProbes(Engine *engine, CommandBuffer *command_buffer)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_radiance_buffer->InsertBarrier(command_buffer, GPUMemory::ResourceState::UNORDERED_ACCESS);

    SubmitPushConstants(engine, command_buffer);

    m_pipeline->Bind(command_buffer);
    
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(),
        command_buffer,
        m_pipeline.get(),
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {.offsets = {0}}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(),
        command_buffer,
        m_pipeline.get(),
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING, .count = 1}
        }
    );

    m_pipeline->TraceRays(
        engine->GetDevice(),
        command_buffer,
        Extent3D(m_grid_info.GetImageDimensions())
    );

    m_radiance_buffer->InsertBarrier(command_buffer, GPUMemory::ResourceState::UNORDERED_ACCESS);
}

void ProbeGrid::ComputeIrradiance(Engine *engine, CommandBuffer *command_buffer)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const auto probe_counts = m_grid_info.NumProbesPerDimension();

    m_irradiance_image->GetGPUImage()->InsertBarrier(
        command_buffer,
        GPUMemory::ResourceState::UNORDERED_ACCESS
    );

    m_depth_image->GetGPUImage()->InsertBarrier(
        command_buffer,
        GPUMemory::ResourceState::UNORDERED_ACCESS
    );
    
    m_update_irradiance->GetPipeline()->Bind(command_buffer);
    
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(),
        command_buffer,
        m_update_irradiance->GetPipeline(),
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING, .count = 1}
        }
    );

    m_update_irradiance->GetPipeline()->Dispatch(command_buffer, Extent3D{{probe_counts.width * probe_counts.height, probe_counts.depth}});

    m_update_depth->GetPipeline()->Bind(command_buffer);
    
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(),
        command_buffer,
        m_update_depth->GetPipeline(),
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING, .count = 1}
        }
    );

    m_update_depth->GetPipeline()->Dispatch(
        command_buffer,
        Extent3D{{probe_counts.width * probe_counts.height, probe_counts.depth}}
    );

    m_irradiance_image->GetGPUImage()->InsertBarrier(
        command_buffer,
        GPUMemory::ResourceState::UNORDERED_ACCESS
    );

    m_depth_image->GetGPUImage()->InsertBarrier(
        command_buffer,
        GPUMemory::ResourceState::UNORDERED_ACCESS
    );
}

} // namespace hyperion::v2