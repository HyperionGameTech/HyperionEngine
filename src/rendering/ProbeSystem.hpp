#ifndef HYPERION_V2_PROBE_SYSTEM_H
#define HYPERION_V2_PROBE_SYSTEM_H

#include <core/Containers.hpp>

#include <rendering/Shader.hpp>
#include <rendering/Compute.hpp>
#include <rendering/rt/TLAS.hpp>

#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <math/BoundingBox.hpp>
#include <Types.hpp>

#include <random>

namespace hyperion::v2 {

using renderer::RaytracingPipeline;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::UniformBuffer;
using renderer::CommandBuffer;
using renderer::ImageDescriptor;
using renderer::Extent3D;
using renderer::Extent2D;
using renderer::Frame;
using renderer::RTUpdateStateFlags;
using renderer::ShaderVec4;

class Engine;

struct alignas(256) ProbeSystemUniforms
{
    Vector4 aabb_max;
    Vector4 aabb_min;
    ShaderVec4<UInt32> probe_border;
    ShaderVec4<UInt32> probe_counts;
    ShaderVec4<UInt32> grid_dimensions;
    ShaderVec4<UInt32> image_dimensions;
    Vector4 params; // probe distance, num rays per probe

    //HYP_PAD_STRUCT_HERE(UInt32, 4);
};

//static_assert(sizeof(ProbeSystemUniforms) == 128);

struct ProbeRayData
{
    Vector4 direction_depth;
    Vector4 origin;
    Vector4 normal;
    Vector4 color;
};

static_assert(sizeof(ProbeRayData) == 64);

struct ProbeGridInfo
{
    static constexpr UInt num_rays_per_probe = 128;
    static constexpr UInt irradiance_octahedron_size = 8;
    static constexpr UInt depth_octahedron_size = 16;

    BoundingBox aabb;
    Extent3D probe_border = { 2, 0, 2 };
    float probe_distance = 60.0f;

    const Vector3 &GetOrigin() const
        { return aabb.min; }

    Extent3D NumProbesPerDimension() const
    {
        const auto probes_per_dimension = MathUtil::Ceil((aabb.GetExtent() / probe_distance) + Vector(probe_border));

        return Extent3D(probes_per_dimension);
    }

    UInt NumProbes() const
    {
        const Extent3D per_dimension = NumProbesPerDimension();

        return per_dimension.width * per_dimension.height * per_dimension.depth;
    }

    Extent2D GetImageDimensions() const
    {
        return Extent2D(MathUtil::NextPowerOf2(NumProbes()), num_rays_per_probe);
    }
};

struct RotationMatrixGenerator
{
    Matrix4 matrix;
    std::random_device random_device;
    std::mt19937 mt { random_device() };
    std::uniform_real_distribution<float> angle { 0.0f, 359.0f };
    std::uniform_real_distribution<float> axis { -1.0f, 1.0f };

    const Matrix4 &Next()
    {
        return matrix = Matrix4::Rotation({
            Vector3 { axis(mt), axis(mt), axis(mt) }.Normalize(),
            MathUtil::DegToRad(angle(mt))
        });
    }
};

struct Probe
{
    Vector3 position;
};

class ProbeGrid
{
public:
    ProbeGrid(ProbeGridInfo &&grid_info);
    ProbeGrid(const ProbeGrid &other) = delete;
    ProbeGrid &operator=(const ProbeGrid &other) = delete;
    ~ProbeGrid();

    void SetTLAS(const Handle<TLAS> &tlas)
        { m_tlas = tlas; }

    void SetTLAS(Handle<TLAS> &&tlas)
        { m_tlas = std::move(tlas); }

    void ApplyTLASUpdates(Engine *engine, RTUpdateStateFlags flags);

    StorageBuffer *GetRadianceBuffer() const { return m_radiance_buffer.get(); }
    StorageImage *GetIrradianceImage() const { return m_irradiance_image.get(); }
    ImageView *GetIrradianceImageView() const { return m_irradiance_image_view.get(); }

    void Init(Engine *engine);
    void Destroy(Engine *engine);

    void RenderProbes(Engine *engine, Frame *frame);
    void ComputeIrradiance(Engine *engine, Frame *frame);

private:
    void CreatePipeline(Engine *engine);
    void CreateComputePipelines(Engine *engine);
    void CreateUniformBuffer(Engine *engine);
    void CreateStorageBuffers(Engine *engine);
    void CreateDescriptorSets(Engine *engine);
    void SubmitPushConstants(Engine *engine, CommandBuffer *command_buffer);

    ProbeGridInfo m_grid_info;
    std::vector<Probe> m_probes;

    Handle<ComputePipeline> m_update_irradiance,
        m_update_depth,
        m_copy_border_texels_irradiance,
        m_copy_border_texels_depth;

    std::unique_ptr<RaytracingPipeline> m_pipeline;
    std::unique_ptr<UniformBuffer> m_uniform_buffer;

    std::unique_ptr<StorageBuffer> m_radiance_buffer;

    std::unique_ptr<StorageImage> m_irradiance_image;
    std::unique_ptr<ImageView> m_irradiance_image_view;

    std::unique_ptr<StorageImage> m_depth_image;
    std::unique_ptr<ImageView> m_depth_image_view;

    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;
    Handle<TLAS> m_tlas;
    FixedArray<bool, max_frames_in_flight> m_has_tlas_updates;

    RotationMatrixGenerator m_random_generator;
    UInt32 m_time;
};

} // namespace hyperion::v2

#endif