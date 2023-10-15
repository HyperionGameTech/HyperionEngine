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
using renderer::CommandBuffer;
using renderer::ImageDescriptor;
;
using renderer::Frame;
using renderer::RTUpdateStateFlags;
using renderer::ShaderVec4;

class Engine;

enum ProbeSystemFlags : UInt32
{
    PROBE_SYSTEM_FLAGS_NONE = 0x0,
    PROBE_SYSTEM_FLAGS_FIRST_RUN = 0x1
};

struct alignas(256) ProbeSystemUniforms
{
    Vector4 aabb_max;
    Vector4 aabb_min;
    ShaderVec4<UInt32> probe_border;
    ShaderVec4<UInt32> probe_counts;
    ShaderVec4<UInt32> grid_dimensions;
    ShaderVec4<UInt32> image_dimensions;
    ShaderVec4<UInt32> params; // x = probe distance, y = num rays per probe, z = flags, w = num bound lights
    UInt32 shadow_map_index;
    UInt32 _pad0, _pad1, _pad2;
    UInt32 light_indices[16];
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
    static constexpr UInt irradiance_octahedron_size = 8;
    static constexpr UInt depth_octahedron_size = 16;
    static constexpr Extent3D probe_border = Extent3D(2, 0, 2);

    BoundingBox aabb;
    Float probe_distance = 3.5f;
    UInt num_rays_per_probe = 128;

    const Vector3 &GetOrigin() const
        { return aabb.min; }

    Extent3D NumProbesPerDimension() const
    {
        const Vector3 probes_per_dimension = MathUtil::Ceil((aabb.GetExtent() / probe_distance) + Vector3(probe_border));

        return Extent3D(probes_per_dimension);
    }

    UInt NumProbes() const
    {
        const Extent3D per_dimension = NumProbesPerDimension();

        return per_dimension.width * per_dimension.height * per_dimension.depth;
    }

    Extent2D GetImageDimensions() const
    {
        return { UInt32(MathUtil::NextPowerOf2(NumProbes())), num_rays_per_probe };
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

    const Array<Probe> &GetProbes() const
        { return m_probes; }

    void SetTLAS(const Handle<TLAS> &tlas)
        { m_tlas = tlas; }

    void SetTLAS(Handle<TLAS> &&tlas)
        { m_tlas = std::move(tlas); }

    void ApplyTLASUpdates(RTUpdateStateFlags flags);

    const GPUBufferRef &GetRadianceBuffer() const
        { return m_radiance_buffer; }

    const ImageRef &GetIrradianceImage() const
        { return m_irradiance_image; }

    const ImageViewRef &GetIrradianceImageView() const
        { return m_irradiance_image_view; }

    void Init();
    void Destroy();

    void SetShadowMapImageView(UInt shadow_map_index, ImageViewRef &&shadow_map_image_view);

    void RenderProbes(Frame *frame);
    void ComputeIrradiance(Frame *frame);

private:
    void CreatePipeline();
    void CreateComputePipelines();
    void CreateUniformBuffer();
    void CreateStorageBuffers();
    void CreateDescriptorSets();
    void UpdateUniforms(Frame *frame);
    void SubmitPushConstants(CommandBuffer *command_buffer);

    ProbeGridInfo m_grid_info;
    Array<Probe> m_probes;
    
    FixedArray<UInt32, max_frames_in_flight> m_updates;

    UInt m_shadow_map_index;
    ImageViewRef m_shadow_map_image_view;

    Handle<ComputePipeline> m_update_irradiance,
        m_update_depth,
        m_copy_border_texels_irradiance,
        m_copy_border_texels_depth;

    Handle<Shader> m_shader;

    RaytracingPipelineRef m_pipeline;
    GPUBufferRef m_uniform_buffer;
    GPUBufferRef m_radiance_buffer;
    ImageRef m_irradiance_image;
    ImageViewRef m_irradiance_image_view;
    ImageRef m_depth_image;
    ImageViewRef m_depth_image_view;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_descriptor_sets;

    Handle<TLAS> m_tlas;

    ProbeSystemUniforms m_uniforms;

    RotationMatrixGenerator m_random_generator;
    UInt32 m_time;
};

} // namespace hyperion::v2

#endif