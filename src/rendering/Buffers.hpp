/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUFFERS_HPP
#define HYPERION_BUFFERS_HPP

#include <core/memory/MemoryPool.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/utilities/Range.hpp>

#include <core/IDGenerator.hpp>

#include <core/Defines.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/Platform.hpp>

#include <core/math/Matrix4.hpp>

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

struct alignas(16) ImmediateDrawShaderData
{
    Matrix4 transform;
    uint32  color_packed;
    uint32  env_probe_type;
    uint32  env_probe_index;
};

static_assert(sizeof(ImmediateDrawShaderData) == 80);

struct alignas(256) SH9Buffer
{
    Vec4f   values[16];
};

static_assert(sizeof(SH9Buffer) == 256);

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
    float  min_roughness;
    Vec2i  output_image_resolution;
    uint32 light_indices[16];
};

/* max number of entities, based on size in mb */
static const SizeType max_entities = (32ull * 1024ull * 1024ull) / sizeof(EntityShaderData);
static const SizeType max_entities_bytes = max_entities * sizeof(EntityShaderData);

class GPUBufferHolderBase
{
protected:
    template <class T>
    GPUBufferHolderBase(TypeWrapper<T>)
        : m_struct_type_id(TypeID::ForType<T>()),
          m_struct_size(sizeof(T))
    {
    }

public:
    virtual ~GPUBufferHolderBase();

    HYP_FORCE_INLINE TypeID GetStructTypeID() const
        { return m_struct_type_id; }

    HYP_FORCE_INLINE SizeType GetStructSize() const
        { return m_struct_size; }

    HYP_FORCE_INLINE SizeType GetGPUBufferOffset(uint32 element_index) const
        { return m_struct_size * element_index; }

    virtual SizeType Count() const = 0;

    virtual uint32 NumElementsPerBlock() const = 0;

    HYP_FORCE_INLINE const GPUBufferRef &GetBuffer(uint32 frame_index) const
        { return m_buffers[frame_index]; }

    virtual void MarkDirty(uint32 index) = 0;

    virtual void UpdateBufferSize(uint32 frame_index) = 0;
    virtual void UpdateBufferData(uint32 frame_index) = 0;

    /*! \brief Copy an element from the GPU back to the CPU side buffer.
     * \param frame_index The index of the frame to copy the element from.
     * \param index The index of the element to copy.
     * \param dst The destination pointer to copy the element to.
     */
    virtual void ReadbackElement(uint32 frame_index, uint32 index, void *dst) = 0;

    virtual uint32 AcquireIndex(void **out_element_ptr = nullptr) = 0;
    virtual void ReleaseIndex(uint32 index) = 0;

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

    HYP_FORCE_INLINE void Set(uint32 index, const void *ptr, SizeType size)
    {
        AssertThrowMsg(size == m_struct_size, "Size does not match the expected size! Size = %llu, Expected = %llu", size, m_struct_size);

        Set_Internal(index, ptr);
    }

protected:
    void CreateBuffers(GPUBufferType type, SizeType count, SizeType size, SizeType alignment = 0);

    virtual void *Get_Internal(uint32 index) = 0;
    virtual void Set_Internal(uint32 index, const void *ptr) = 0;

    TypeID                                          m_struct_type_id;
    SizeType                                        m_struct_size;

    FixedArray<GPUBufferRef, max_frames_in_flight>  m_buffers;
    FixedArray<Range<uint32>, max_frames_in_flight> m_dirty_ranges;
};

template <class StructType>
class GPUBufferHolderMemoryPool final : public MemoryPool<StructType>
{
public:
    using Base = MemoryPool<StructType>;

    GPUBufferHolderMemoryPool(uint32 initial_count = Base::InitInfo::num_initial_elements)
        : Base(initial_count, /* create_initial_blocks */ true, /* block_init_ctx */ nullptr)
    {
    }

    HYP_FORCE_INLINE void MarkDirty(uint32 index)
    {
        for (auto &it : m_dirty_ranges) {
            it |= { index, index + 1 };
        }
    }

    void SetElement(uint32 index, const StructType &value)
    {
        Base::SetElement(index, value);

        MarkDirty(index);
    }

    void EnsureGPUBufferCapacity(const GPUBufferRef &buffer, uint32 frame_index)
    {
        bool was_resized = false;
        HYPERION_ASSERT_RESULT(buffer->EnsureCapacity(Base::NumAllocatedElements() * sizeof(StructType), &was_resized));

        if (was_resized) {
            // Reset the dirty ranges
            m_dirty_ranges[frame_index].SetStart(0);
            m_dirty_ranges[frame_index].SetEnd(Base::NumAllocatedElements());
        }
    }

    void CopyToGPUBuffer(const GPUBufferRef &buffer, uint32 frame_index)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        if (!m_dirty_ranges[frame_index]) {
            return;
        }

        const uint32 range_end = m_dirty_ranges[frame_index].GetEnd(),
            range_start = m_dirty_ranges[frame_index].GetStart();

        AssertThrowMsg(buffer->Size() >= range_end * sizeof(StructType),
            "Buffer does not have enough space for the current number of elements! Buffer size = %llu",
            buffer->Size());

        uint32 block_index = 0;

        typename LinkedList<typename Base::Block>::Iterator begin_it = Base::m_blocks.Begin();
        typename LinkedList<typename Base::Block>::Iterator end_it = Base::m_blocks.End();

        for (uint32 block_index = 0; block_index < Base::m_num_blocks.Get(MemoryOrder::ACQUIRE) && begin_it != end_it; ++block_index, ++begin_it) {
            if (block_index < range_start / Base::num_elements_per_block) {
                continue;
            }

            if (block_index * Base::num_elements_per_block >= range_end) {
                break;
            }

            const SizeType buffer_size = buffer->Size();

            const uint32 index = block_index * Base::num_elements_per_block;

            const SizeType offset = index;
            const SizeType count = Base::num_elements_per_block;

            // sanity checks
            AssertThrow(offset - index < begin_it->elements.Size());

            AssertThrowMsg(count <= int64(buffer_size / sizeof(StructType)) - int64(offset),
                "Buffer does not have enough space for the current number of elements! Buffer size = %llu, Required size = %llu",
                buffer_size,
                (offset + count) * sizeof(StructType));

            buffer->Copy(
                offset * sizeof(StructType),
                count * sizeof(StructType),
                &begin_it->elements[offset - index]
            );
        }

        m_dirty_ranges[frame_index].Reset();
    }

private:
    // @TODO Make atomic
    FixedArray<Range<uint32>, 2>    m_dirty_ranges;

protected:
    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

template <class StructType, GPUBufferType BufferType>
class GPUBufferHolder final : public GPUBufferHolderBase
{
public:
    GPUBufferHolder(uint32 count)
        : GPUBufferHolderBase(TypeWrapper<StructType> { }),
          m_pool(count)
    {
        GPUBufferHolderBase::CreateBuffers(BufferType, count, sizeof(StructType));
    }

    GPUBufferHolder(const GPUBufferHolder &other)               = delete;
    GPUBufferHolder &operator=(const GPUBufferHolder &other)    = delete;

    virtual ~GPUBufferHolder() override                         = default;

    virtual SizeType Count() const override
    {
        return m_pool.NumAllocatedElements();
    }

    virtual uint32 NumElementsPerBlock() const override
    {
        return m_pool.num_elements_per_block;
    }

    virtual void UpdateBufferSize(uint32 frame_index) override
    {
        m_pool.RemoveEmptyBlocks();
        m_pool.EnsureGPUBufferCapacity(m_buffers[frame_index], frame_index);
    }

    virtual void UpdateBufferData(uint32 frame_index) override
    {
        m_pool.CopyToGPUBuffer(m_buffers[frame_index], frame_index);
    }

    virtual void MarkDirty(uint32 index) override
    {
        m_pool.MarkDirty(index);
    }

    virtual void ReadbackElement(uint32 frame_index, uint32 index, void *dst) override
    {
        AssertThrowMsg(index < m_pool.NumAllocatedElements(), "Index out of bounds! Index = %u, Size = %u", index, m_pool.NumAllocatedElements());

        m_buffers[frame_index]->Read(sizeof(StructType) * index, sizeof(StructType), dst);
    }

    HYP_FORCE_INLINE uint32 AcquireIndex(StructType **out_element_ptr)
    {
        return m_pool.AcquireIndex(out_element_ptr);
    }

    virtual uint32 AcquireIndex(void **out_element_ptr = nullptr) override
    {
        StructType *element_ptr;
        const uint32 index = m_pool.AcquireIndex(&element_ptr);

        if (out_element_ptr != nullptr) {
            *out_element_ptr = element_ptr;
        }

        return index;
    }

    virtual void ReleaseIndex(uint32 batch_index) override
    {
        return m_pool.ReleaseIndex(batch_index);
    }

    /*! \brief Get a reference to an object in the _current_ staging buffer,
     * use when it is preferable to fetch the object, update the struct, and then
     * call Set. This is usually when the object would have a large stack size
     */
    HYP_FORCE_INLINE StructType &Get(uint32 index)
    {
        return m_pool.GetElement(index);
    }

    HYP_FORCE_INLINE void Set(uint32 index, const StructType &value)
    {
        m_pool.SetElement(index, value);
    }
    
private:
    virtual void *Get_Internal(uint32 index) override
    {
        return &Get(index);
    }

    virtual void Set_Internal(uint32 index, const void *ptr) override
    {
        Set(index, *static_cast<const StructType *>(ptr));
    }

    GPUBufferHolderMemoryPool<StructType>   m_pool;
};

} // namespace hyperion

#endif
