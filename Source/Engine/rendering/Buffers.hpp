/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/memory/allocator/SlabAllocator.hpp>
#include <Core/memory/Memory.hpp>
#include <Core/memory/Pimpl.hpp>

#include <Core/threading/DataRaceDetector.hpp>

#include <Core/containers/String.hpp>
#include <Core/containers/FixedArray.hpp>

#include <Core/utilities/IdGenerator.hpp>
#include <Core/utilities/Range.hpp>
#include <Core/reflection/TypeInfoFwd.hpp>

#include <Core/Defines.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/Frame.hpp>

#include <Core/math/Mat4f.hpp>

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

struct alignas(16) ParticleShaderData
{
    Vec4f position;   //   4 x 4 = 16
    Vec4f velocity;   // + 4 x 4 = 32
    Vec4f color;      // + 4 x 4 = 48
    Vec4f attributes; // + 4 x 4 = 64
};

static_assert(sizeof(ParticleShaderData) == 64);

struct CubemapUniforms
{
    Mat4f projectionMatrices[6];
    Mat4f viewMatrices[6];
};

static_assert(sizeof(CubemapUniforms) % 256 == 0);

struct ImmediateDrawShaderData
{
    Mat4f transform;
    uint32 colorPacked;
    uint32 envProbeType;
    uint32 envProbeIndex;
    uint32 idx; // ~0u == culled
};

static_assert(sizeof(ImmediateDrawShaderData) == 80);

struct SH9Buffer
{
    Vec4f values[16];
};

static_assert(sizeof(SH9Buffer) == 256);

struct SHTile
{
    Vec4f coeffsWeights[9];
};

static_assert(sizeof(SHTile) == 144);

struct VoxelUniforms
{
    Vec4f extent;
    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4u dimensions; // num mipmaps stored in w component
};

static_assert(sizeof(VoxelUniforms) == 64);

struct BlueNoiseBuffer
{
    Vec4i sobol256spp256d[256 * 256 / 4];
    Vec4i scramblingTile[128 * 128 * 8 / 4];
    Vec4i rankingTile[128 * 128 * 8 / 4];
};

struct RayTracingConstants
{
    uint32 numBoundLights;
    uint32 numBoundEnvProbes;
    uint32 rayOffset; // for lightmapper
    float minRoughness;

    Vec2i outputImageResolution;
    Vec2i _pad;
};

class StagingBufferPool
{
public:
    StagingBufferPool();
    
    void OnFrameStart();
    void OnFrameEnd();

    void Cleanup(uint32 frameIndex);

    GpuBuffer* AcquireStagingBuffer(uint32 bufferSize);

private:
    Pimpl<struct StagingBufferPoolImpl> m_impl;
};

class GpuBufferHolderBase
{
protected:
    GpuBufferHolderBase(const TypeInfo* structTypeInfo, bool cpuAccessible)
        : m_structTypeInfo(structTypeInfo),
          m_cpuAccessible(cpuAccessible)
    {
        AssertDebug(m_structTypeInfo != nullptr);
    }

public:
    virtual ~GpuBufferHolderBase();

    HYP_FORCE_INLINE const TypeInfo* GetStructTypeInfo() const
    {
        return m_structTypeInfo;
    }

    virtual size_t Count() const = 0;

    virtual uint32 NumElementsPerBlock() const = 0;

    HYP_FORCE_INLINE const GpuBufferRef& GetBuffer(uint32 frameIndex) const
    {
        return m_gpuBuffer;
    }

    virtual void MarkDirty(uint32 index) = 0;

    virtual void UpdateBufferSize(uint32 frameIndex) = 0;

    virtual void UpdateBufferData(
        uint32 frameIndex,
        StagingBufferPool& stagingBufferPool,
        CommandRecorder& cr) = 0;

    virtual uint32 AcquireIndex(void** outElementPtr = nullptr) = 0;
    virtual void ReleaseIndex(uint32 index) = 0;

    // Ensures capacity for the given index.
    virtual void EnsureCapacity(uint32 index) = 0;

    void WriteBufferData(uint32 index, const void* ptr, size_t size)
    {
        AssertDebug(size == TypeInfo_GetSize(*m_structTypeInfo), "Size does not match the expected size! Size = {}, Expected = {}", size, TypeInfo_GetSize(*m_structTypeInfo));

        WriteBufferData_Internal(index, ptr);
    }

    static void WriteBufferData_Static(GpuBufferHolderBase* gpuBufferHolder, uint32 index, void* bufferDataPtr, size_t bufferSize)
    {
        AssertDebug(gpuBufferHolder != nullptr);
        AssertDebug(bufferSize == TypeInfo_GetSize(*gpuBufferHolder->m_structTypeInfo),
            "Size does not match the expected size! Size = %llu, Expected = %llu",
            bufferSize,
            TypeInfo_GetSize(*gpuBufferHolder->m_structTypeInfo));

        gpuBufferHolder->WriteBufferData_Internal(index, bufferDataPtr);
    }

    virtual void* GetCpuMapping(uint32 index) = 0;

    void DeleteBuffer()
    {
        m_gpuBuffer.Reset();
    }

protected:
    void CreateBuffers(GpuBufferType bufferType, size_t count, size_t size);
    void CopyStagingToGpu(
        uint32 frameIndex, CommandRecorder& cr,
        Span<GpuBuffer* const> stagingBuffers,
        Span<const uint32> chunkStarts,
        Span<const uint32> chunkEnds);

    virtual void WriteBufferData_Internal(uint32 index, const void* ptr) = 0;

    const TypeInfo* m_structTypeInfo;
    bool m_cpuAccessible;

    GpuBufferRef m_gpuBuffer;
};

template <class StructType>
struct GpuBufferHolderMemoryPoolInitInfo
{
    static constexpr uint32 numBytesPerBlock = MathUtil::Max(MathUtil::NextPowerOf2(sizeof(StructType)), 1u << 14);
    static constexpr uint32 numElementsPerBlock = numBytesPerBlock / sizeof(StructType);
    static constexpr uint32 numInitialElements = numElementsPerBlock;
};

template <class StructType>
class GpuBufferHolderMemoryPool final
{
public:
    using InitInfo = GpuBufferHolderMemoryPoolInitInfo<StructType>;

    static constexpr uint32 numElementsPerBlock = InitInfo::numElementsPerBlock;

    GpuBufferHolderMemoryPool(Name poolName, uint32 initialCount = InitInfo::numInitialElements)
        : m_allocator(
            InitInfo::numBytesPerBlock,
            MathUtil::Max(uint32(alignof(StructType)), 16u),
            4)
    {
        const uint32 numInitialBlocks = (initialCount + numElementsPerBlock - 1) / numElementsPerBlock;

        for (uint32 i = 0; i < numInitialBlocks; ++i)
        {
            AllocateNewBlock();
        }

        for (Range<uint32>& dirtyRange : m_dirtyRanges)
        {
            dirtyRange = { 0, MathUtil::Max(initialCount, 1u) };
        }
    }

    GpuBufferHolderMemoryPool(const GpuBufferHolderMemoryPool&) = delete;
    GpuBufferHolderMemoryPool& operator=(const GpuBufferHolderMemoryPool&) = delete;

    ~GpuBufferHolderMemoryPool() = default;

    HYP_FORCE_INLINE uint32 NumAllocatedElements() const
    {
        return uint32(m_blocks.Size()) * numElementsPerBlock;
    }

    HYP_FORCE_INLINE void MarkDirty(uint32 index)
    {
        for (auto& it : m_dirtyRanges)
        {
            it |= { index, index + 1 };
        }
    }

    void SetElement(uint32 index, const StructType& value)
    {
        GetElement(index) = value;

        MarkDirty(index);
    }

    StructType& GetElement(uint32 index)
    {
        const uint32 blockIndex = index / numElementsPerBlock;
        const uint32 elementIndex = index % numElementsPerBlock;

        AssertDebug(blockIndex < m_blocks.Size());

        return m_blocks[blockIndex][elementIndex];
    }

    uint32 AcquireIndex(StructType** outElementPtr = nullptr)
    {
        AssertOnThread(g_renderThread);

        const uint32 index = m_idGenerator.Next() - 1;
        const uint32 blockIndex = index / numElementsPerBlock;

        while (blockIndex >= m_blocks.Size())
        {
            AllocateNewBlock();
        }

        if (outElementPtr != nullptr)
        {
            *outElementPtr = &m_blocks[blockIndex][index % numElementsPerBlock];
        }

        return index;
    }

    void ReleaseIndex(uint32 index)
    {
        m_idGenerator.ReleaseId(index + 1);
    }

    void EnsureCapacity(uint32 index)
    {
        const uint32 requiredBlocks = (index + numElementsPerBlock) / numElementsPerBlock;

        while (requiredBlocks > m_blocks.Size())
        {
            AllocateNewBlock();
        }
    }

    void EnsureGpuBufferCapacity(const GpuBufferRef& buffer, uint32 frameIndex)
    {
        bool wasResized = false;
        CheckResult(buffer->EnsureCapacity(NumAllocatedElements() * sizeof(StructType), &wasResized));

        if (wasResized)
        {
            // if resized, we need to copy all data again
            m_dirtyRanges[frameIndex].SetStart(0);
            m_dirtyRanges[frameIndex].SetEnd(NumAllocatedElements());
        }
    }

    /*! \brief Builds the necessary staging buffer updates for the dirty ranges.
     *  If the buffer is CPU accessible, no staging buffers will be created and the data is directly copied to the GPU buffer.
     *  The resulting \p outChunkStarts, \p outChunkEnds, and \p outStagingBuffers can be used to perform the actual copy operation.
     */
    void BuildUpdates(
        uint32 frameIndex,
        StagingBufferPool& stagingBufferPool,
        GpuBuffer* dstBuffer,
        Array<uint32, RenderAllocator>& outChunkStarts,
        Array<uint32, RenderAllocator>& outChunkEnds,
        Array<GpuBuffer*, RenderAllocator>& outStagingBuffers)
    {
        AssertOnThread(g_renderThread);

        if (!m_dirtyRanges[frameIndex])
        {
            return;
        }

        const uint32 rangeStart = m_dirtyRanges[frameIndex].GetStart();
        const uint32 rangeEnd = m_dirtyRanges[frameIndex].GetEnd();

        const uint32 numBlocks = uint32(m_blocks.Size());

        // Collect all dirty blocks first
        struct DirtyBlockInfo
        {
            uint32 blockIndex;
            uint32 bufferOffset;
            uint32 bufferSize;
            void* dataPtr;
        };

        Array<DirtyBlockInfo, RenderAllocator> dirtyBlocks;
        dirtyBlocks.Reserve((rangeEnd - rangeStart) / numElementsPerBlock + 1);

        for (uint32 blockIndex = 0; blockIndex < numBlocks; ++blockIndex)
        {
            if (blockIndex < rangeStart / numElementsPerBlock)
            {
                continue;
            }

            if (blockIndex * numElementsPerBlock >= rangeEnd)
            {
                break;
            }

            const uint32 offset = blockIndex * numElementsPerBlock;
            const uint32 bufferOffset = offset * uint32(sizeof(StructType));
            const uint32 bufferSize = numElementsPerBlock * uint32(sizeof(StructType));

            dirtyBlocks.PushBack({ blockIndex,
                bufferOffset,
                bufferSize,
                m_blocks[blockIndex]
            });
        }

        if (dirtyBlocks.Empty())
        {
            m_dirtyRanges[frameIndex].Reset();
            return;
        }

        // dirty copy + flush for cpu accessible
        if (false) // TEMP debugging. dstBuffer->IsCpuAccessible())
        {
            for (DirtyBlockInfo& dirtyBlock : dirtyBlocks)
            {
                dstBuffer->Copy(dirtyBlock.bufferOffset, dirtyBlock.bufferSize, dirtyBlock.dataPtr);
                dstBuffer->Flush(dirtyBlock.bufferOffset, dirtyBlock.bufferSize);
            }

            m_dirtyRanges[frameIndex].Reset();

            return;
        }

        for (DirtyBlockInfo& dirtyBlock : dirtyBlocks)
        {
            GpuBuffer* stagingBuffer = stagingBufferPool.AcquireStagingBuffer(dirtyBlock.bufferSize);
            Assert(stagingBuffer != nullptr && stagingBuffer->IsCreated());

            stagingBuffer->Copy(0, dirtyBlock.bufferSize, dirtyBlock.dataPtr);

            outStagingBuffers.PushBack(stagingBuffer);
            outChunkStarts.PushBack(dirtyBlock.bufferOffset);
            outChunkEnds.PushBack(dirtyBlock.bufferOffset + dirtyBlock.bufferSize);
        }

        m_dirtyRanges[frameIndex].Reset();
    }

private:
    void AllocateNewBlock()
    {
        void* blockMemory = m_allocator.Allocate();
        Assert(blockMemory != nullptr);

        Memory::Fill(blockMemory, 0, InitInfo::numBytesPerBlock);

        m_blocks.PushBack(static_cast<StructType*>(blockMemory));
    }

    TSlabAllocator<RenderAllocator> m_allocator;

    Array<StructType*> m_blocks;
    IdGenerator m_idGenerator;

    FixedArray<Range<uint32>, NumFramesInFlight> m_dirtyRanges;
};

template <class StructType, GpuBufferType BufferType>
class GpuBufferHolder final : public GpuBufferHolderBase
{
public:
    GpuBufferHolder(uint32 initialCount, bool cpuAccessible)
        : GpuBufferHolderBase(&TypeOf<StructType>(), cpuAccessible),
          m_pool(NAME_FMT("GpuBufferData_{}", TypeInfo_GetName(*m_structTypeInfo)), initialCount)
    {
        GpuBufferHolderBase::CreateBuffers(BufferType, initialCount, sizeof(StructType));
    }

    GpuBufferHolder(const GpuBufferHolder& other) = delete;
    GpuBufferHolder& operator=(const GpuBufferHolder& other) = delete;

    virtual ~GpuBufferHolder() override = default;

    virtual size_t Count() const override
    {
        return m_pool.NumAllocatedElements();
    }

    virtual uint32 NumElementsPerBlock() const override
    {
        return m_pool.numElementsPerBlock;
    }

    virtual void UpdateBufferSize(uint32 frameIndex) override
    {
        // m_pool.RemoveEmptyBlocks();

        m_pool.EnsureGpuBufferCapacity(m_gpuBuffer, frameIndex);
    }

    virtual void UpdateBufferData(
        uint32 frameIndex,
        StagingBufferPool& stagingBufferPool,
        CommandRecorder& cr) override
    {
        Array<uint32, RenderAllocator> chunkStarts;
        Array<uint32, RenderAllocator> chunkEnds;
        Array<GpuBuffer*, RenderAllocator> stagingBuffers;

        m_pool.BuildUpdates(
            frameIndex,
            stagingBufferPool,
            m_gpuBuffer,
            chunkStarts,
            chunkEnds,
            stagingBuffers);

        if (stagingBuffers.Empty())
        {
            return;
        }

        // sanity check, ensure that the chunks are in ascending order
        for (size_t i = 1; i < chunkStarts.Size(); ++i)
        {
            AssertDebug(chunkStarts[i] >= chunkEnds[i - 1]);
        }

        // sanity check, ensure that the chunks are within bounds of our main gpu buffer
        const size_t gpuBufferSize = m_gpuBuffer->Size();

        for (size_t i = 0; i < chunkStarts.Size(); ++i)
        {
            AssertDebug(chunkStarts[i] < gpuBufferSize);
            AssertDebug(chunkEnds[i] <= gpuBufferSize);
        }

        GpuBufferHolderBase::CopyStagingToGpu(
            frameIndex, cr,
            stagingBuffers.ToSpan(),
            chunkStarts.ToSpan(),
            chunkEnds.ToSpan());
    }

    virtual void MarkDirty(uint32 index) override
    {
        m_pool.MarkDirty(index);
    }

    HYP_FORCE_INLINE uint32 AcquireIndex(StructType** outElementPtr)
    {
        return m_pool.AcquireIndex(outElementPtr);
    }

    virtual uint32 AcquireIndex(void** outElementPtr = nullptr) override
    {
        StructType* elementPtr;
        const uint32 index = m_pool.AcquireIndex(&elementPtr);

        if (outElementPtr != nullptr)
        {
            *outElementPtr = elementPtr;
        }

        return index;
    }

    virtual void ReleaseIndex(uint32 batchIndex) override
    {
        return m_pool.ReleaseIndex(batchIndex);
    }

    virtual void EnsureCapacity(uint32 index) override
    {
        m_pool.EnsureCapacity(index);
    }

    HYP_FORCE_INLINE void WriteBufferData(uint32 index, const StructType& value)
    {
        m_pool.SetElement(index, value);
    }

    virtual void* GetCpuMapping(uint32 index) override
    {
        AssertDebug(index < m_pool.NumAllocatedElements(), "Index out of bounds! Index = {}, Size = {}", index, m_pool.NumAllocatedElements());

        return &m_pool.GetElement(index);
    }

private:
    virtual void WriteBufferData_Internal(uint32 index, const void* bufferDataPtr) override
    {
        WriteBufferData(index, *reinterpret_cast<const StructType*>(bufferDataPtr));
    }

    GpuBufferHolderMemoryPool<StructType> m_pool;
};

} // namespace Hyperion