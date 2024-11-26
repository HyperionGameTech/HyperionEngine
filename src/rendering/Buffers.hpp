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

static constexpr SizeType max_probes_in_sh_grid_buffer = max_bound_ambient_probes;

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

struct alignas(16) ImmediateDrawShaderData
{
    Matrix4 transform;
    uint32  color_packed;
    uint32  probe_type;
    uint32  probe_id;
};

static_assert(sizeof(ImmediateDrawShaderData) == 80);

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

struct alignas(16) RTRadianceUniforms
{
    uint32 num_bound_lights;
    uint32 ray_offset; // for lightmapper
    Vec2i  output_image_resolution;
    uint32 light_indices[16];
};

static_assert(sizeof(RTRadianceUniforms) == 80);

/* max number of entities, based on size in mb */
static const SizeType max_entities = (32ull * 1024ull * 1024ull) / sizeof(EntityShaderData);
static const SizeType max_entities_bytes = max_entities * sizeof(EntityShaderData);
/* max number of shadow maps, based on size in kb */
static const SizeType max_shadow_maps = (4ull * 1024ull) / sizeof(ShadowShaderData);
static const SizeType max_shadow_maps_bytes = max_shadow_maps * sizeof(ShadowShaderData);

class GPUBufferHolderBase
{
protected:
    GPUBufferHolderBase(TypeID struct_type_id)
        : m_struct_type_id(struct_type_id)
    {
    }

public:
    virtual ~GPUBufferHolderBase();

    virtual uint32 Count() const = 0;

    HYP_FORCE_INLINE const GPUBufferRef &GetBuffer(uint32 frame_index) const
        { return m_buffers[frame_index]; }

    virtual void MarkDirty(uint32 index) = 0;
    virtual void ResetElement(uint32 index) = 0;
    virtual void UpdateBuffer(Device *device, uint32 frame_index) = 0;

    template <class T>
    HYP_FORCE_INLINE T &Get(uint32 index)
    {
        AssertThrowMsg(TypeID::ForType<T>() == m_struct_type_id, "T does not match the expected type!");

        return *static_cast<T *>(Get_Internal(index));
    }

    template <class T>
    HYP_FORCE_INLINE void Set(uint32 index, const T &value)
    {
        AssertThrowMsg(TypeID::ForType<T>() == m_struct_type_id, "T does not match the expected type!");

        Set_Internal(index, &value);
    }

protected:
    void CreateBuffers(SizeType count, SizeType size, SizeType alignment = 0);

    virtual void *Get_Internal(uint32 index) = 0;
    virtual void Set_Internal(uint32 index, const void *ptr) = 0;

    TypeID                                          m_struct_type_id;
    FixedArray<GPUBufferRef, max_frames_in_flight>  m_buffers;
    FixedArray<Range<uint32>, max_frames_in_flight> m_dirty_ranges;
};

HYP_DISABLE_OPTIMIZATION;
template <class StructType>
class BufferList
{
    static constexpr uint32 num_elements_per_block = 16;

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
    static constexpr uint32 s_invalid_index = ~0u;

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
        for (auto &it : m_dirty_ranges) {
            it |= { index, index + 1 };
        }
    }

    uint32 AcquireIndex()
    {
        const uint32 index = m_id_generator.NextID() - 1;

        const uint32 block_index = index / num_elements_per_block;

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
        m_id_generator.FreeID(index + 1);

        const uint32 block_index = index / num_elements_per_block;

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
        AssertThrow(index < m_blocks.Size() * num_elements_per_block);

        const uint32 block_index = index / num_elements_per_block;
        const uint32 element_index = index % num_elements_per_block;

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
        AssertThrow(index < m_blocks.Size() * num_elements_per_block);

        const uint32 block_index = index / num_elements_per_block;
        const uint32 element_index = index % num_elements_per_block;

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

    void CopyToGPUBuffer(Device *device, const GPUBufferRef &buffer, uint32 frame_index)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        const uint32 range_end = m_dirty_ranges[frame_index].GetEnd(),
            range_start = m_dirty_ranges[frame_index].GetStart();

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

            if (block_index * num_elements_per_block >= range_end) {
                break;
            }

            uint32 index = block_index * num_elements_per_block;

            uint32 offset = (range_start > index)
                ? range_start
                : index;

            uint32 count = (range_end > (block_index + 1) * num_elements_per_block)
                ? num_elements_per_block
                : range_end - offset;

            // sanity checks
            AssertThrow(offset - index < begin_it->data.Size());
            AssertThrow((offset + count) * sizeof(StructType) <= buffer->Size());

            buffer->Copy(
                device,
                offset * sizeof(StructType),
                count * sizeof(StructType),
                &begin_it->data[offset - index]
            );
        }

        m_dirty_ranges[frame_index].Reset();
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
    uint32                          m_initial_num_blocks;

    LinkedList<Block>               m_blocks;
    AtomicVar<uint32>               m_num_blocks;
    // Needs to be locked when accessing blocks beyond initial_num_blocks or adding/removing blocks.
    Mutex                           m_blocks_mutex;
    // @TODO Make atomic
    FixedArray<Range<uint32>, 2>    m_dirty_ranges;

    IDGenerator                     m_id_generator;

#ifdef HYP_ENABLE_MT_CHECK
    DataRaceDetector                m_data_race_detector;
#endif
};
HYP_ENABLE_OPTIMIZATION;

template <class StructType, GPUBufferType BufferType>
class GPUBufferHolder final : public GPUBufferHolderBase
{
public:
    GPUBufferHolder(uint32 count)
        : GPUBufferHolderBase(TypeID::ForType<StructType>()),
          m_buffer_list(count)
    {
        for (uint32 frame_index = 0; frame_index < m_buffers.Size(); frame_index++) {
            m_buffers[frame_index] = MakeRenderObject<GPUBuffer>(BufferType);
        }
        GPUBufferHolderBase::CreateBuffers(count, sizeof(StructType));
    }

    GPUBufferHolder(const GPUBufferHolder &other)               = delete;
    GPUBufferHolder &operator=(const GPUBufferHolder &other)    = delete;

    virtual ~GPUBufferHolder() override                         = default;

    virtual uint32 Count() const override
    {
        return m_buffer_list.NumElements();
    }

    virtual void UpdateBuffer(Device *device, uint32 frame_index) override
    {
        m_buffer_list.RemoveEmptyBlocks();
        m_buffer_list.CopyToGPUBuffer(device, m_buffers[frame_index], frame_index);
    }

    virtual void MarkDirty(uint32 index) override
    {
        m_buffer_list.MarkDirty(index);
    }

    virtual void ResetElement(uint32 index) override
    {
        Set(index, { });
    }

    /*! \brief Get a reference to an object in the _current_ staging buffer,
     * use when it is preferable to fetch the object, update the struct, and then
     * call Set. This is usually when the object would have a large stack size
     */
    HYP_FORCE_INLINE StructType &Get(uint32 index)
    {
        return m_buffer_list.GetElement(index);
    }

    HYP_FORCE_INLINE void Set(uint32 index, const StructType &value)
    {
        m_buffer_list.SetElement(index, value);
    }

    uint32 AcquireIndex()
    {
        return m_buffer_list.AcquireIndex();
    }

    void ReleaseIndex(uint32 batch_index)
    {
        return m_buffer_list.ReleaseIndex(batch_index);
    }
    
private:
    virtual void *Get_Internal(uint32 index) override
    {
        return &Get(index);
    }

    virtual void Set_Internal(uint32 index, const void *ptr)
    {
        Set(index, *static_cast<const StructType *>(ptr));
    }

    BufferList<StructType>              m_buffer_list;
};

} // namespace hyperion

#endif
