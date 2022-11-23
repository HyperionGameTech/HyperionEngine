#ifndef HYPERION_V2_BUFFERS_H
#define HYPERION_V2_BUFFERS_H

#include <rendering/DrawProxy.hpp>
#include <rendering/Light.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <math/Rect.hpp>
#include <math/Matrix4.hpp>

#include <util/Defines.hpp>

#include <core/lib/HeapArray.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Range.hpp>
#include <core/lib/FixedArray.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <memory>
#include <atomic>
#include <climits>

#define HYP_BUFFERS_USE_SPINLOCK 0

namespace hyperion::renderer {

class Device;

} // namespace hyperion::renderer

namespace hyperion::v2 {

using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;
using renderer::PerFrameData;
using renderer::Device;
using renderer::ShaderVec2;
using renderer::ShaderVec3;
using renderer::ShaderVec4;
using renderer::ShaderMat4;

struct alignas(256) CubemapUniforms
{
    Matrix4 projection_matrices[6];
    Matrix4 view_matrices[6];
};

static_assert(sizeof(CubemapUniforms) % 256 == 0);

struct SkeletonShaderData
{
    static constexpr SizeType max_bones = 128;

    Matrix4 bones[max_bones];
};

static_assert(sizeof(SkeletonShaderData) % 256 == 0);

struct alignas(256) ObjectShaderData
{
    ShaderMat4 model_matrix;
    ShaderMat4 previous_model_matrix;

    ShaderVec4<Float32> _pad0;
    ShaderVec4<Float32> _pad1;
    ShaderVec4<Float32> world_aabb_max;
    ShaderVec4<Float32> world_aabb_min;

    UInt32 entity_id;
    UInt32 scene_id;
    UInt32 mesh_id;
    UInt32 material_id;

    UInt32 skeleton_id;
    UInt32 bucket;
};

static_assert(sizeof(ObjectShaderData) == 256);

struct MaterialShaderData
{
    static constexpr UInt max_bound_textures = 16u;
    
    ShaderVec4<Float32> albedo;
    
    // 4 vec4s of 0.0..1.0 values stuffed into uint32s
    ShaderVec4<UInt32> packed_params;
    
    ShaderVec2<Float32> uv_scale;
    Float32 parallax_height;
    Float32 _pad0;
    
    UInt32 texture_index[16];
    
    UInt32 texture_usage;
    UInt32 _pad1;
    UInt32 _pad2;
    UInt32 _pad3;
};

static_assert(sizeof(MaterialShaderData) == 128);

struct alignas(256) SceneShaderData
{
    Matrix4 view;
    Matrix4 projection;
    Matrix4 previous_view;

    ShaderVec4<Float32> camera_position;
    ShaderVec4<Float32> camera_direction;
    ShaderVec4<Float32> camera_up;

    float camera_near;
    float camera_far;
    float camera_fov;
    float _pad0;

    UInt32 environment_texture_index;
    UInt32 environment_texture_usage;
    UInt32 resolution_x;
    UInt32 resolution_y;
    
    ShaderVec4<Float32> aabb_max;
    ShaderVec4<Float32> aabb_min;

    Float32 global_timer;
    UInt32 frame_counter;
    UInt32 num_lights;
    UInt32 enabled_render_components_mask;

    ShaderVec4<Float32> taa_params;
    ShaderVec4<Float32> fog_params;
};

static_assert(sizeof(SceneShaderData) == 512);

struct alignas(16) ShadowShaderData
{
    Matrix4 projection;
    Matrix4 view;
    UInt32 scene_index;
};

//static_assert(sizeof(ShadowShaderData) == 128);

struct alignas(16) EnvProbeShaderData
{
    ShaderVec4<Float> aabb_max;
    ShaderVec4<Float> aabb_min;
    ShaderVec4<Float> world_position;
    UInt32 texture_index;
    UInt32 flags;
};

struct alignas(256) ImmediateDrawShaderData
{
    ShaderMat4 transform;
    UInt32 color_packed;
};

struct ObjectInstance
{
    UInt32 entity_id;
    UInt32 draw_command_index;
    UInt32 batch_index;
    UInt32 num_indices;

    ShaderVec4<Float> aabb_max;
    ShaderVec4<Float> aabb_min;
    ShaderVec4<UInt32> packed_data;
};

static_assert(sizeof(ObjectInstance) == 64);

/* max number of skeletons, based on size in mb */
static const SizeType max_skeletons = (8ull * 1024ull * 1024ull) / sizeof(SkeletonShaderData);
static const SizeType max_skeletons_bytes = max_skeletons * sizeof(SkeletonShaderData);
/* max number of materials, based on size in mb */
static const SizeType max_materials = (8ull * 1024ull * 1024ull) / sizeof(MaterialShaderData);
static const SizeType max_materials_bytes = max_materials * sizeof(MaterialShaderData);
/* max number of objects, based on size in mb */
static const SizeType max_objects = (32ull * 1024ull * 1024ull) / sizeof(ObjectShaderData);
static const SizeType max_objects_bytes = max_materials * sizeof(ObjectShaderData);
/* max number of scenes (cameras, essentially), based on size in kb */
static const SizeType max_scenes = (32ull * 1024ull) / sizeof(SceneShaderData);
static const SizeType max_scenes_bytes = max_scenes * sizeof(SceneShaderData);
/* max number of lights, based on size in kb */
static const SizeType max_lights = (16ull * 1024ull) / sizeof(LightDrawProxy);
static const SizeType max_lights_bytes = max_lights * sizeof(LightDrawProxy);
/* max number of shadow maps, based on size in kb */
static const SizeType max_shadow_maps = (16ull * 1024ull) / sizeof(ShadowShaderData);
static const SizeType max_shadow_maps_bytes = max_shadow_maps * sizeof(ShadowShaderData);
/* max number of env probes, based on size in kb */
static const SizeType max_env_probes = (16ull * 1024ull) / sizeof(EnvProbeShaderData);
static const SizeType max_env_probes_bytes = max_env_probes * sizeof(EnvProbeShaderData);
/* max number of immediate drawn objects, based on size in mb */
static const SizeType max_immediate_draws = (1ull * 1024ull * 1024ull) / sizeof(ImmediateDrawShaderData);
static const SizeType max_immediate_draws_bytes = max_immediate_draws * sizeof(ImmediateDrawShaderData);

template <class Buffer, class StructType, SizeType Size>
class ShaderData
{
public:
    ShaderData(SizeType num_buffers)
    {
        m_buffers.resize(num_buffers);

        for (SizeType i = 0; i < num_buffers; i++) {
            m_buffers[i] = std::make_unique<Buffer>();
        }

        for (SizeType i = 0; i < m_staging_objects_pool.num_staging_buffers; i++) {
            auto &buffer = m_staging_objects_pool.buffers[i];

            AssertThrow(buffer.dirty.Size() == num_buffers);

            for (auto &d : buffer.dirty) {
                d.SetStart(0);
                d.SetEnd(Size);
            }
        }
    }

    ShaderData(const ShaderData &other) = delete;
    ShaderData &operator=(const ShaderData &other) = delete;
    ~ShaderData() = default;

    const auto &GetBuffer(UInt frame_index) const
        { return m_buffers[frame_index]; }

    const auto &GetBuffers() const { return m_buffers; }
    
    void Create(Device *device)
    {
        for (SizeType i = 0; i < m_buffers.size(); i++) {
            HYPERION_ASSERT_RESULT(m_buffers[i]->Create(device, sizeof(StructType) * Size));
        }
    }

    void Destroy(Device *device)
    {
        for (SizeType i = 0; i < m_buffers.size(); i++) {
            HYPERION_ASSERT_RESULT(m_buffers[i]->Destroy(device));
        }
    }

    void UpdateBuffer(Device *device, SizeType buffer_index)
    {
#if HYP_BUFFERS_USE_SPINLOCK
        static constexpr UInt32 max_spins = 2;

        for (UInt32 spin_count = 0; spin_count < max_spins; spin_count++) {
            auto &current = m_staging_objects_pool.Current();

            if (!current.locked) {
                current.PerformUpdate(
                    device,
                    m_buffers,
                    buffer_index,
                    current.objects.Data()
                );
                
                m_staging_objects_pool.Next();

                return;
            }

            m_staging_objects_pool.Next();
        }
        
        DebugLog(
            LogType::Warn,
            "Buffer update spinlock exceeded maximum of %lu -- for %s\n",
            max_spins,
            typeid(StructType).name()
        );
#else
        auto &current = m_staging_objects_pool.Current();

        current.PerformUpdate(
            device,
            m_buffers,
            buffer_index,
            current.objects.Data()
        );
#endif
    }

    void Set(SizeType index, const StructType &value)
    {
        m_staging_objects_pool.Set(index, value);
    }

    /*! \brief Get a reference to an object in the _current_ staging buffer,
     * use when it is preferable to fetch the object, update the struct, and then
     * call Set. This is usually when the object would have a large stack size
     */
    StructType &Get(SizeType index)
    {
        return m_staging_objects_pool.Current().objects[index];
    }
    
private:
    struct StagingObjectsPool
    {
        static constexpr UInt32 num_staging_buffers = HYP_BUFFERS_USE_SPINLOCK ? 2 : 1;

        StagingObjectsPool() = default;
        StagingObjectsPool(const StagingObjectsPool &other) = delete;
        StagingObjectsPool &operator=(const StagingObjectsPool &other) = delete;
        StagingObjectsPool(StagingObjectsPool &other) = delete;
        StagingObjectsPool &operator=(StagingObjectsPool &&other) = delete;
        ~StagingObjectsPool() = default;

        struct StagingObjects
        {
#if HYP_BUFFERS_USE_SPINLOCK
            std::atomic_bool locked { false };
#endif
            HeapArray<StructType, Size> objects;
            FixedArray<Range<SizeType>, max_frames_in_flight> dirty;

            StagingObjects() = default;
            StagingObjects(const StagingObjects &other) = delete;
            StagingObjects &operator=(const StagingObjects &other) = delete;
            StagingObjects(StagingObjects &other) = delete;
            StagingObjects &operator=(StagingObjects &&other) = delete;
            ~StagingObjects() = default;

            template <class BufferContainer>
            void PerformUpdate(Device *device, BufferContainer &buffer_container, SizeType buffer_index, StructType *ptr)
            {
                // TODO: Time it, see if we can afford to copy multiple small sub-sections
                // of the buffer..

                auto &current_dirty = dirty[buffer_index];

                const auto dirty_end = current_dirty.GetEnd(),
                    dirty_start = current_dirty.GetStart();

                if (dirty_end <= dirty_start) {
                    return;
                }

                const auto dirty_distance = dirty_end - dirty_start;

                buffer_container[buffer_index]->Copy(
                    device,
                    dirty_start * sizeof(StructType),
                    dirty_distance * sizeof(StructType),
                    &ptr[dirty_start]
                );

                current_dirty.Reset();
            }

            void MarkDirty(SizeType index)
            {
                for (auto &d : dirty) {
                    d |= Range<SizeType> { index, index + 1 };
                }
            }

        } buffers[num_staging_buffers];
        
#if HYP_BUFFERS_USE_SPINLOCK
        std::atomic_uint32_t current_index{0};
#endif

        StagingObjects &Current()
        {

#if HYP_BUFFERS_USE_SPINLOCK
            return buffers[current_index];
#else
            return buffers[0];
#endif
        }

#if HYP_BUFFERS_USE_SPINLOCK
        void Next()
        {
            current_index = (current_index + 1) % num_staging_buffers;
        }
#endif

        void Set(SizeType index, const StructType &value)
        {
            AssertThrowMsg(index < buffers[0].objects.Size(), "Cannot set shader data at %llu in buffer: out of bounds", index);

#if HYP_BUFFERS_USE_SPINLOCK
            for (UInt32 i = 0; i < num_staging_buffers; i++) {
                const auto staging_object_index = (current_index + i) % num_staging_buffers;
                auto &staging_object = buffers[staging_object_index];

                staging_object.locked = true;
                staging_object.objects[index] = value;
                staging_object.MarkDirty(index);
                staging_object.locked = false;
            }
#else
            auto &staging_object = buffers[0];
            
            staging_object.objects[index] = value;
            staging_object.MarkDirty(index);
#endif
        }
    };

    std::vector<std::unique_ptr<Buffer>> m_buffers;
    StagingObjectsPool m_staging_objects_pool;
};

} // namespace hyperion::v2

#endif
