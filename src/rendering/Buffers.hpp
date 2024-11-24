/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUFFERS_HPP
#define HYPERION_BUFFERS_HPP

#include <core/Handle.hpp>

#include <core/containers/HeapArray.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/utilities/Range.hpp>

#include <core/IDGenerator.hpp>

#include <core/Defines.hpp>

#include <rendering/DrawProxy.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/Platform.hpp>

#include <math/Matrix4.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <mutex>

#define HYP_RENDER_OBJECT_OFFSET(cls, index) \
    (uint32((index) * sizeof(cls ## ShaderData)))

namespace hyperion::renderer {

namespace platform {
template <PlatformType PLATFORM>
class Device;
} // namespace platform

using Device = platform::Device<Platform::CURRENT>;

} // namespace hyperion::renderer

namespace hyperion {

class Engine;

extern HYP_API Handle<Engine>   g_engine;

using renderer::GPUBufferType;

static constexpr SizeType max_entities_per_instance_batch = 60;
static constexpr SizeType max_probes_in_sh_grid_buffer = max_bound_ambient_probes;

enum EnvGridType : uint32
{
    ENV_GRID_TYPE_INVALID   = uint32(-1),
    ENV_GRID_TYPE_SH        = 0,
    ENV_GRID_TYPE_MAX
};

struct alignas(16) EntityInstanceBatch
{
    uint32  num_entities;
    uint32  _pad0;
    uint32  _pad1;
    uint32  _pad2;
    uint32  indices[max_entities_per_instance_batch];
    Matrix4 transforms[max_entities_per_instance_batch];
};

static_assert(sizeof(EntityInstanceBatch) == 4096);

struct alignas(16) ParticleShaderData
{
    Vec4f   position;   //   4 x 4 = 16
    Vec4f   velocity;   // + 4 x 4 = 32
    Vec4f   color;      // + 4 x 4 = 48
    Vec4f   attributes; // + 4 x 4 = 64
};

static_assert(sizeof(ParticleShaderData) == 64);

struct alignas(16) GaussianSplattingInstanceShaderData {
    Vec4f   position;   //   4 x 4 = 16
    Vec4f   rotation;   // + 4 x 4 = 32
    Vec4f   scale;      // + 4 x 4 = 48
    Vec4f   color;      // + 4 x 4 = 64
};

static_assert(sizeof(GaussianSplattingInstanceShaderData) == 64);

struct alignas(16) GaussianSplattingSceneShaderData {
    Matrix4 model_matrix;
};

static_assert(sizeof(GaussianSplattingSceneShaderData) == 64);

struct alignas(256) CubemapUniforms
{
    Matrix4 projection_matrices[6];
    Matrix4 view_matrices[6];
};

static_assert(sizeof(CubemapUniforms) % 256 == 0);

struct alignas(256) SkeletonShaderData
{
    static constexpr SizeType max_bones = 256;

    Matrix4 bones[max_bones];
};

static_assert(sizeof(SkeletonShaderData) % 256 == 0);

enum EntityGPUDataFlags : uint32
{
    ENTITY_GPU_FLAG_NONE            = 0x0,
    ENTITY_GPU_FLAG_HAS_SKELETON    = 0x1
};

struct alignas(16) EntityUserData
{
    Vec4u   user_data0;
    Vec4u   user_data1;
};

struct alignas(256) EntityShaderData
{
    Matrix4         model_matrix;
    Matrix4         previous_model_matrix;

    Vec4f           _pad0;
    Vec4f           _pad1;
    Vec4f           world_aabb_max;
    Vec4f           world_aabb_min;

    uint32          entity_index;
    uint32          _unused;
    uint32          material_index;
    uint32          skeleton_index;

    uint32          bucket;
    uint32          flags;
    uint32          _pad3;
    uint32          _pad4;

    EntityUserData  user_data;
};

static_assert(sizeof(EntityShaderData) == 256);

struct MaterialShaderData
{
    Vec4f               albedo;
    
    // 4 vec4s of 0.0..1.0 values stuffed into uint32s
    Vec4u               packed_params;
    
    Vec2f               uv_scale;
    float               parallax_height;
    float               _pad0;
    
    uint32              texture_index[16];
    
    uint32              texture_usage;
    uint32              _pad1;
    uint32              _pad2;
    uint32              _pad3;
};

static_assert(sizeof(MaterialShaderData) == 128);

struct SceneShaderData
{
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4f   fog_params;

    float   game_time;
    uint32  frame_counter;
    uint32  enabled_render_components_mask;
    uint32  enabled_environment_maps_mask;

    HYP_PAD_STRUCT_HERE(uint8, 64 + 128);
};

static_assert(sizeof(SceneShaderData) == 256);

struct alignas(256) CameraShaderData
{
    Matrix4     view;
    Matrix4     projection;
    Matrix4     previous_view;

    Vec4u       dimensions;
    Vec4f       camera_position;
    Vec4f       camera_direction;
    Vec4f       jitter;
    
    float       camera_near;
    float       camera_far;
    float       camera_fov;
    float       _pad0;
};

static_assert(sizeof(CameraShaderData) == 512);

struct alignas(256) EnvGridShaderData
{
    uint32  probe_indices[max_bound_ambient_probes];

    Vec4f   center;
    Vec4f   extent;
    Vec4f   aabb_max;
    Vec4f   aabb_min;

    Vec4u   density;
    Vec4u   enabled_indices_mask;

    Vec4f   voxel_grid_aabb_max;
    Vec4f   voxel_grid_aabb_min;
};

static_assert(sizeof(EnvGridShaderData) == 4352);

struct alignas(256) ShadowShaderData
{
    Matrix4 projection;
    Matrix4 view;
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec2u   dimensions;
    uint32  flags;
};

static_assert(sizeof(ShadowShaderData) == 256);

struct alignas(256) EnvProbeShaderData
{
    Matrix4 face_view_matrices[6];

    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4f   world_position;

    uint32  texture_index;
    uint32  flags;
    float   camera_near;
    float   camera_far;

    Vec2u   dimensions;
    Vec2u   _pad2;

    Vec4i   position_in_grid;
    Vec4i   position_offset;
    Vec4u   _pad5;
};

static_assert(sizeof(EnvProbeShaderData) == 512);

struct alignas(16) ImmediateDrawShaderData
{
    Matrix4 transform;
    uint32  color_packed;
    uint32  probe_type;
    uint32  probe_id;
};

static_assert(sizeof(ImmediateDrawShaderData) == 80);

struct alignas(16) ObjectInstance
{
    uint32  entity_id;
    uint32  draw_command_index;
    uint32  instance_index;
    uint32  batch_index;
};

static_assert(sizeof(ObjectInstance) == 16);

struct alignas(128) LightShaderData
{
    uint32  light_id;
    uint32  light_type;
    uint32  color_packed;
    float   radius;
    // 16

    float   falloff;
    uint32  shadow_map_index;
    Vec2f   area_size;
    // 32

    Vec4f   position_intensity;
    Vec4f   normal;
    // 64

    Vec2f   spot_angles;
    uint32  material_id;
    uint32  _pad2;

    Vec4u   pad3;
    Vec4u   pad4;
    Vec4u   pad5;
};

static_assert(sizeof(LightShaderData) == 128);

struct alignas(256) SH9Buffer
{
    Vec4f   values[16];
};

static_assert(sizeof(SH9Buffer) == 256);

struct alignas(256) SHGridBuffer
{
    Vec4f   color_values[max_probes_in_sh_grid_buffer * 9];
};

// static_assert(sizeof(SHGridBuffer) == 9216);

struct alignas(16) EnvGridProbeDataBuffer
{
    static constexpr Vec2u probe_storage_resolution = { 64, 64 };

    Vec2u   data[probe_storage_resolution.x * probe_storage_resolution.y][max_bound_ambient_probes];
};

struct alignas(16) SHTile
{
    Vec4f   coeffs_weights[9];
};

static_assert(sizeof(SHTile) == 144);

struct alignas(16) VoxelUniforms
{
    Vec4f   extent;
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4u   dimensions; // num mipmaps stored in w component
};

static_assert(sizeof(VoxelUniforms) == 64);

struct BlueNoiseBuffer
{
    Vec4i   sobol_256spp_256d[256 * 256 / 4];
    Vec4i   scrambling_tile[128 * 128 * 8 / 4];
    Vec4i   ranking_tile[128 * 128 * 8 / 4];
};

struct alignas(16) PostProcessingUniforms
{
    Vec2u   effect_counts; // pre, post
    Vec2u   last_enabled_indices; // pre, post
    Vec2u   masks; // pre, post
};

static_assert(sizeof(PostProcessingUniforms) == 32);

struct alignas(256) DDGIUniforms
{
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4u   probe_border;
    Vec4u   probe_counts;
    Vec4u   grid_dimensions;
    Vec4u   image_dimensions;
    Vec4u   params; // x = probe distance, y = num rays per probe, z = flags, w = num bound lights
    uint32  shadow_map_index;
    uint32  _pad0, _pad1, _pad2;
    uint32  light_indices[16];
    //HYP_PAD_STRUCT_HERE(uint32, 4);
};

static_assert(sizeof(DDGIUniforms) == 256);


struct alignas(16) RTRadianceUniforms
{
    uint32 num_bound_lights;
    uint32 ray_offset; // for lightmapper
    uint32 _pad1, _pad2;
    uint32 light_indices[16];
};

static_assert(sizeof(RTRadianceUniforms) == 80);

/* max number of skeletons, based on size in mb */
static const SizeType max_skeletons = (8ull * 1024ull * 1024ull) / sizeof(SkeletonShaderData);
static const SizeType max_skeletons_bytes = max_skeletons * sizeof(SkeletonShaderData);
/* max number of materials, based on size in mb */
static const SizeType max_materials = (8ull * 1024ull * 1024ull) / sizeof(MaterialShaderData);
static const SizeType max_materials_bytes = max_materials * sizeof(MaterialShaderData);
/* max number of entities, based on size in mb */
static const SizeType max_entities = (32ull * 1024ull * 1024ull) / sizeof(EntityShaderData);
static const SizeType max_entities_bytes = max_entities * sizeof(EntityShaderData);
/* max number of scenes, based on size in kb */
static const SizeType max_scenes = (32ull * 1024ull) / sizeof(SceneShaderData);
static const SizeType max_scenes_bytes = max_scenes * sizeof(SceneShaderData);
/* max number of cameras, based on size in kb */
static const SizeType max_cameras = (16ull * 1024ull) / sizeof(CameraShaderData);
static const SizeType max_cameras_bytes = max_cameras * sizeof(CameraShaderData);
/* max number of lights, based on size in kb */
static const SizeType max_lights = (64ull * 1024ull) / sizeof(LightShaderData);
static const SizeType max_lights_bytes = max_lights * sizeof(LightShaderData);
/* max number of shadow maps, based on size in kb */
static const SizeType max_shadow_maps = (4ull * 1024ull) / sizeof(ShadowShaderData);
static const SizeType max_shadow_maps_bytes = max_shadow_maps * sizeof(ShadowShaderData);
/* max number of env probes, based on size in mb */
static const SizeType max_env_probes = (8ull * 1024ull * 1024ull) / sizeof(EnvProbeShaderData);
static const SizeType max_env_probes_bytes = max_env_probes * sizeof(EnvProbeShaderData);
/* max number of env grids, based on size in mb */
static const SizeType max_env_grids = (1ull * 1024ull * 1024ull) / sizeof(EnvGridShaderData);
static const SizeType max_env_grids_bytes = max_env_grids * sizeof(EnvGridShaderData);
/* max number of instance batches, based on size in mb */
static const SizeType max_entity_instance_batches = (128ull * 1024ull * 1024ull) / sizeof(EntityInstanceBatch);
static const SizeType max_entity_instance_batches_bytes = max_entity_instance_batches * sizeof(EntityInstanceBatch);

template <class T>
using BufferTicket = uint;

class GPUBufferHolderBase
{
public:
    virtual ~GPUBufferHolderBase();

    virtual uint32 Count() const = 0;

    HYP_FORCE_INLINE const GPUBufferRef &GetBuffer(uint32 frame_index) const
        { return m_buffers[frame_index]; }

    /*HYP_FORCE_INLINE void MarkDirty(SizeType index)
    {
        // @TODO Ensure thread safety
        for (auto &dirty_range : m_dirty_ranges) {
            dirty_range |= Range<uint32> { uint32(index), uint32(index + 1) };
        }
    }*/

    virtual void MarkDirty(uint32 index) = 0;
    virtual void ResetElement(uint32 index) = 0;
    virtual void UpdateBuffer(Device *device, uint32 frame_index) = 0;

protected:
    void CreateBuffers(SizeType count, SizeType size, SizeType alignment = 0);

    FixedArray<GPUBufferRef, max_frames_in_flight>  m_buffers;
    FixedArray<Range<uint32>, max_frames_in_flight> m_dirty_ranges;
};

// @TODO Implement to allow dynamic resizing of buffers
template <class StructType>
class BufferList
{
    static constexpr uint32 num_elements_per_block = 128;

    struct Block
    {
        FixedArray<StructType, num_elements_per_block>          data;
        AtomicVar<uint32>                                       num_elements { 0 };

#ifdef HYP_ENABLE_MT_CHECK
        FixedArray<DataRaceDetector, num_elements_per_block>    data_race_detectors;
#endif

        Block()
        {
            Memory::MemSet(data.Data(), 0, sizeof(StructType) * num_elements_per_block);
        }

        HYP_FORCE_INLINE bool IsEmpty() const
            { return num_elements.Get(MemoryOrder::ACQUIRE) == 0; }
    };

public:
    static constexpr uint32 s_invalid_index = 0;

    BufferList(uint32 initial_count = 16 * num_elements_per_block)
        : m_initial_num_blocks((initial_count + num_elements_per_block - 1) / num_elements_per_block),
          m_num_blocks(m_initial_num_blocks)
    {
        for (uint32 i = 0; i < m_initial_num_blocks; i++) {
            m_blocks.EmplaceBack();
        }
    }

    HYP_FORCE_INLINE uint32 NumElements() const
    {
        return m_num_blocks.Get(MemoryOrder::ACQUIRE) * num_elements_per_block;
    }

    HYP_FORCE_INLINE void MarkDirty(uint32 index)
    {
        if (index == s_invalid_index) {
            return;
        }

        m_dirty_range |= { index - 1, index };
    }

    uint32 AcquireIndex()
    {
        const uint32 index = m_id_generator.NextID();

        const uint32 block_index = (index - 1) / num_elements_per_block;

        if (block_index < m_initial_num_blocks) {
            Block &block = m_blocks[block_index];
            block.num_elements.Increment(1, MemoryOrder::RELEASE);
        } else {
            Mutex::Guard guard(m_blocks_mutex);

            if (index < num_elements_per_block * m_num_blocks.Get(MemoryOrder::ACQUIRE)) {
                Block &block = m_blocks[block_index];
                block.num_elements.Increment(1, MemoryOrder::RELEASE);
            } else {
                // Add blocks until we can insert the element
                while (index >= num_elements_per_block * m_num_blocks.Get(MemoryOrder::ACQUIRE)) {
                    m_blocks.EmplaceBack();
                    m_num_blocks.Increment(1, MemoryOrder::RELEASE);
                }

                Block &block = m_blocks[block_index];
                block.num_elements.Increment(1, MemoryOrder::RELEASE);
            }
        }

        return index;
    }

    void ReleaseIndex(uint32 index)
    {
        if (index == s_invalid_index) {
            return;
        }

        m_id_generator.FreeID(index);

        const uint32 block_index = (index - 1) / num_elements_per_block;

        if (block_index < m_initial_num_blocks) {
            Block &block = m_blocks[block_index];

            block.num_elements.Decrement(1, MemoryOrder::RELEASE);
        } else {
            Mutex::Guard guard(m_blocks_mutex);

            AssertThrow(index < num_elements_per_block * m_num_blocks.Get(MemoryOrder::ACQUIRE));

            Block &block = m_blocks[block_index];
            block.num_elements.Decrement(1, MemoryOrder::RELEASE);
        }
    }

    StructType &GetElement(uint32 index)
    {
        const uint32 block_index = (index - 1) / num_elements_per_block;
        const uint32 element_index = (index - 1) % num_elements_per_block;

        if (block_index < m_initial_num_blocks) {
            Block &block = m_blocks[block_index];
            //HYP_MT_CHECK_READ(block.data_race_detectors[element_index]);

            return block.data[element_index];
        } else {
            Mutex::Guard guard(m_blocks_mutex);

            Block &block = m_blocks[block_index];
            //HYP_MT_CHECK_READ(block.data_race_detectors[element_index]);
            
            return block.data[element_index];
        }
    }

    void SetElement(uint32 index, const StructType &value)
    {
        AssertThrow(index != s_invalid_index);

        const uint32 block_index = (index - 1) / num_elements_per_block;
        const uint32 element_index = (index - 1) % num_elements_per_block;

        if (block_index < m_initial_num_blocks) {
            Block &block = m_blocks[block_index];
            //HYP_MT_CHECK_RW(block.data_race_detectors[element_index]);
                
            block.data[element_index] = value;
        } else {
            Mutex::Guard guard(m_blocks_mutex);

            AssertThrow(block_index < m_num_blocks.Get(MemoryOrder::ACQUIRE));

            Block &block = m_blocks[block_index];
            //HYP_MT_CHECK_RW(block.data_race_detectors[element_index]);
                
            block.data[element_index] = value;
        }

        MarkDirty(index);
    }

    void CopyToGPUBuffer(Device *device, const GPUBufferRef &buffer)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        const uint32 range_end = m_dirty_range.GetEnd(),
            range_start = m_dirty_range.GetStart();

        if (range_end <= range_start) {
            return;
        }

        AssertThrowMsg(buffer->Size() >= range_end * sizeof(StructType),
            "Buffer does not have enough space for the current number of elements! Buffer size = %llu",
            buffer->Size());

        uint32 block_index = 0;

        typename LinkedList<Block>::Iterator begin_it = m_blocks.Begin();
        typename LinkedList<Block>::Iterator end_it = m_blocks.End();

        for (uint32 block_index = 0; block_index < m_num_blocks.Get(MemoryOrder::ACQUIRE) && begin_it != end_it; ++block_index, ++begin_it) {
            if (block_index < range_start / num_elements_per_block) {
                continue;
            }

            if (block_index * num_elements_per_block > range_end) {
                break;
            }

            const uint32 offset = range_start > (block_index * num_elements_per_block) ? (range_start - (block_index * num_elements_per_block)) : 0;
            const uint32 count = MathUtil::Min(range_end - range_start, num_elements_per_block - offset);

            buffer->Copy(
                device,
                (offset + (block_index * num_elements_per_block)) * sizeof(StructType),
                count * sizeof(StructType),
                &begin_it->data[offset]
            );
        }

        m_dirty_range.Reset();
    }

    /*! \brief Remove empty blocks from the back of the list */
    void RemoveEmptyBlocks()
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        if (m_num_blocks.Get(MemoryOrder::ACQUIRE) <= m_initial_num_blocks) {
            return;
        }

        Mutex::Guard guard(m_blocks_mutex);

        typename LinkedList<Block>::Iterator begin_it = m_blocks.Begin();
        typename LinkedList<Block>::Iterator end_it = m_blocks.End();

        Array<typename LinkedList<Block>::Iterator> to_remove;

        for (uint32 block_index = 0; block_index < m_num_blocks.Get(MemoryOrder::ACQUIRE) && begin_it != end_it; ++block_index, ++begin_it) {
            if (block_index < m_initial_num_blocks) {
                continue;
            }

            if (begin_it->IsEmpty()) {
                to_remove.PushBack(begin_it);
            } else {
                to_remove.Clear();
            }
        }

        if (to_remove.Any()) {
            m_num_blocks.Decrement(to_remove.Size(), MemoryOrder::RELEASE);

            while (to_remove.Any()) {
                m_blocks.Erase(to_remove.PopBack());
            }
        }
    }

private:
    uint32              m_initial_num_blocks;

    LinkedList<Block>   m_blocks;
    AtomicVar<uint32>   m_num_blocks;
    // Needs to be locked when accessing blocks beyond initial_num_blocks or adding/removing blocks.
    Mutex               m_blocks_mutex;
    // @TODO Make atomic
    Range<uint32>       m_dirty_range;

    IDGenerator         m_id_generator;

#ifdef HYP_ENABLE_MT_CHECK
    DataRaceDetector    m_data_race_detector;
#endif
};

template <class StructType, GPUBufferType BufferType, uint32 TCount = ~0u>
class GPUBufferHolder final : public GPUBufferHolderBase
{
public:
    GPUBufferHolder()
        : GPUBufferHolder(TCount)
    {
        AssertThrowMsg(TCount != ~0u, "Count must be provided as a template argument to use the default constructor!");
    }

    GPUBufferHolder(uint32 count)
        : m_count(count),
          m_buffer_list(count)
    {
        m_objects.Resize(count);

#ifdef HYP_ENABLE_MT_CHECK
        m_data_race_detectors.Resize(count);
#endif

        for (uint32 frame_index = 0; frame_index < m_buffers.Size(); frame_index++) {
            m_buffers[frame_index] = MakeRenderObject<GPUBuffer>(BufferType);

            m_dirty_ranges[frame_index].SetStart(0);
            m_dirty_ranges[frame_index].SetEnd(count);
        }

        GPUBufferHolderBase::CreateBuffers(count, sizeof(StructType));
    }

    GPUBufferHolder(const GPUBufferHolder &other)               = delete;
    GPUBufferHolder &operator=(const GPUBufferHolder &other)    = delete;

    virtual ~GPUBufferHolder() override                         = default;

    virtual uint32 Count() const override
    {
        return m_buffer_list.NumElements();
        //return m_count;
    }

    virtual void UpdateBuffer(Device *device, uint32 frame_index) override
    {
        /*const uint32 range_end = m_dirty_ranges[frame_index].GetEnd(),
            range_start = m_dirty_ranges[frame_index].GetStart();

        if (range_end <= range_start) {
            return;
        }

        const uint32 range_distance = range_end - range_start;

        m_buffers[frame_index]->Copy(
            device,
            range_start * sizeof(StructType),
            range_distance * sizeof(StructType),
            &m_objects.Data()[range_start]
        );

        m_dirty_ranges[frame_index].Reset();*/


        // Temp
        m_buffer_list.RemoveEmptyBlocks();
        m_buffer_list.CopyToGPUBuffer(device, m_buffers[frame_index]);
    }

    virtual void MarkDirty(uint32 index) override
    {
        // temp
        m_buffer_list.MarkDirty(index + 1);
    }

    virtual void ResetElement(uint32 index) override
    {
        Set(index, { });
    }

    HYP_FORCE_INLINE void Set(uint32 index, const StructType &value)
    {
        /*AssertThrowMsg(index < m_objects.Size(), "Cannot set shader data at %llu in buffer: out of bounds", index);

        HYP_MT_CHECK_RW(m_data_race_detectors[index]);

        m_objects[index] = value;

        MarkDirty(index);*/

        // temp
        m_buffer_list.SetElement(index + 1, value);
    }

    /*! \brief Get a reference to an object in the _current_ staging buffer,
     * use when it is preferable to fetch the object, update the struct, and then
     * call Set. This is usually when the object would have a large stack size
     */
    HYP_FORCE_INLINE StructType &Get(uint32 index)
    {
        /*AssertThrowMsg(index < m_objects.Size(), "Cannot get shader data at %llu in buffer: out of bounds", index);

        HYP_MT_CHECK_READ(m_data_race_detectors[index]);

        return m_objects[index];*/
        

        // temp
        return m_buffer_list.GetElement(index + 1);
    }

    BufferTicket<StructType> AcquireTicket()
    {
        // temp
        return m_buffer_list.AcquireIndex();

        Mutex::Guard guard(m_mutex);

        if (m_free_indices.Any()) {
            return m_free_indices.Pop();
        }

        return m_current_index++;
    }

    void ReleaseTicket(BufferTicket<StructType> batch_index)
    {
        // temp
        return m_buffer_list.ReleaseIndex(batch_index);


        if (batch_index == 0) {
            return;
        }

        Mutex::Guard guard(m_mutex);

        m_free_indices.Push(batch_index);
    }
    
private:

    uint32                              m_count;
    
    Array<StructType>                   m_objects;

#ifdef HYP_ENABLE_MT_CHECK
    Array<DataRaceDetector>             m_data_race_detectors;
#endif

    BufferTicket<StructType>            m_current_index = 1; // reserve first index (0)
    Queue<BufferTicket<StructType>>     m_free_indices;
    Mutex                               m_mutex;


    // Testing, WIP
    BufferList<StructType>              m_buffer_list;
};

} // namespace hyperion

#endif
