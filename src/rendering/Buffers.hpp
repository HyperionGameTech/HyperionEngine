/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/MemoryPool.hpp>
#include <core/memory/Pimpl.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/Range.hpp>

#include <core/Defines.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderFrame.hpp>

#include <core/math/Matrix4.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>

namespace hyperion {

static constexpr SizeType maxProbesInShGridBuffer = g_maxBoundAmbientProbes;

struct alignas(16) ParticleShaderData
{
    Vec4f position;   //   4 x 4 = 16
    Vec4f velocity;   // + 4 x 4 = 32
    Vec4f color;      // + 4 x 4 = 48
    Vec4f attributes; // + 4 x 4 = 64
};

static_assert(sizeof(ParticleShaderData) == 64);

struct alignas(16) GaussianSplattingInstanceShaderData
{
    Vec4f position; //   4 x 4 = 16
    Vec4f rotation; // + 4 x 4 = 32
    Vec4f scale;    // + 4 x 4 = 48
    Vec4f color;    // + 4 x 4 = 64
};

static_assert(sizeof(GaussianSplattingInstanceShaderData) == 64);

struct GaussianSplattingSceneShaderData
{
    Matrix4 modelMatrix;
};

static_assert(sizeof(GaussianSplattingSceneShaderData) == 64);

struct CubemapUniforms
{
    Matrix4 projectionMatrices[6];
    Matrix4 viewMatrices[6];
};

static_assert(sizeof(CubemapUniforms) % 256 == 0);

struct ImmediateDrawShaderData
{
    Matrix4 transform;
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

struct RTRadianceUniforms
{
    uint32 numBoundLights;
    uint32 rayOffset; // for lightmapper
    float minRoughness;
    float _pad0;
    Vec2i outputImageResolution;
    float _pad1;
    float _pad2;
    alignas(Vec4f) uint32 lightIndices[16];
};


class StagingBufferPool
{
public:
    HYP_API static StagingBufferPool& GetInstance();

    HYP_API StagingBufferPool();

    HYP_API void Cleanup(uint32 frameIndex);
    HYP_API GpuBufferBase* AcquireStagingBuffer(uint32 frameIndex, uint32 offset, uint32 bufferSize);

private:
    Pimpl<struct StagingBufferPoolImpl> m_impl;
};

class GpuBufferHolderBase
{
protected:
    template <class T>
    GpuBufferHolderBase(TypeWrapper<T>)
        : m_structTypeId(TypeId::ForType<T>()),
          m_structSize(sizeof(T))
    {
    }

public:
    virtual ~GpuBufferHolderBase();

    HYP_FORCE_INLINE TypeId GetStructTypeId() const
    {
        return m_structTypeId;
    }

    HYP_FORCE_INLINE SizeType GetStructSize() const
    {
        return m_structSize;
    }

    HYP_FORCE_INLINE SizeType GetGpuBufferOffset(uint32 elementIndex) const
    {
        return m_structSize * elementIndex;
    }

    virtual SizeType Count() const = 0;

    virtual uint32 NumElementsPerBlock() const = 0;

    HYP_FORCE_INLINE const GpuBufferRef& GetBuffer(uint32 frameIndex) const
    {
        return m_gpuBuffer;
    }

    virtual void MarkDirty(uint32 index) = 0;

    virtual void UpdateBufferSize(uint32 frameIndex) = 0;
    virtual void UpdateBufferData(FrameBase* frame) = 0;

    virtual uint32 AcquireIndex(void** outElementPtr = nullptr) = 0;
    virtual void ReleaseIndex(uint32 index) = 0;

    // Ensures capacity for the given index.
    virtual void EnsureCapacity(uint32 index) = 0;

    void WriteBufferData(uint32 index, const void* ptr, SizeType size)
    {
        AssertDebug(size == m_structSize, "Size does not match the expected size! Size = {}, Expected = {}", size, m_structSize);

        WriteBufferData_Internal(index, ptr);
    }

    static void WriteBufferData_Static(GpuBufferHolderBase* gpuBufferHolder, uint32 index, void* bufferDataPtr, SizeType bufferSize)
    {
        AssertDebug(gpuBufferHolder != nullptr);
        AssertDebug(bufferSize == gpuBufferHolder->m_structSize,
            "Size does not match the expected size! Size = %llu, Expected = %llu",
            bufferSize,
            gpuBufferHolder->m_structSize);

        gpuBufferHolder->WriteBufferData_Internal(index, bufferDataPtr);
    }

    virtual void* GetCpuMapping(uint32 index) = 0;

protected:
    void CreateBuffers(GpuBufferType bufferType, SizeType count, SizeType size);
    void CopyToGpuBuffer(
        FrameBase* frame,
        const Array<GpuBufferBase*>& stagingBuffers,
        const Array<uint32>& chunkStarts,
        const Array<uint32>& chunkEnds);

    virtual void WriteBufferData_Internal(uint32 index, const void* ptr) = 0;

    TypeId m_structTypeId;
    SizeType m_structSize;

    GpuBufferRef m_gpuBuffer;
};


// Specialization for Memory pool init info to allocate larger blocks:
template <class StructType>
struct GpuBufferHolderMemoryPoolInitInfo
{
    static constexpr uint32 numBytesPerBlock = MathUtil::Max(MathUtil::NextPowerOf2(sizeof(StructType)), 1u << 14); // minimum 16KB blocks
    static constexpr uint32 numElementsPerBlock = numBytesPerBlock / sizeof(StructType);
    static constexpr uint32 numInitialElements = numElementsPerBlock;
};

template <class StructType>
class GpuBufferHolderMemoryPool final : public MemoryPool<StructType, GpuBufferHolderMemoryPoolInitInfo<StructType>>
{
public:
    using Base = MemoryPool<StructType, GpuBufferHolderMemoryPoolInitInfo<StructType>>;

    GpuBufferHolderMemoryPool(Name poolName, uint32 initialCount = Base::InitInfo::numInitialElements)
        : Base(poolName, initialCount, /* createInitialBlocks */ true, /* blockInitCtx */ nullptr)
    {
        for (Range<uint32>& dirtyRange : m_dirtyRanges)
        {
            dirtyRange = { 0, MathUtil::Max(initialCount, 1) };
        }
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
        Base::SetElement(index, value);

        MarkDirty(index);
    }

    void EnsureGpuBufferCapacity(const GpuBufferRef& buffer, uint32 frameIndex)
    {
        bool wasResized = false;
        HYP_GFX_ASSERT(buffer->EnsureCapacity(Base::NumAllocatedElements() * sizeof(StructType), &wasResized));

        if (wasResized)
        {
            // if resized, we need to copy all data again
            m_dirtyRanges[frameIndex].SetStart(0);
            m_dirtyRanges[frameIndex].SetEnd(Base::NumAllocatedElements());
        }
    }

    void BuildStagingBuffers(uint32 frameIndex, Array<GpuBufferBase*>& outStagingBuffers, Array<uint32>& outChunkStarts, Array<uint32>& outChunkEnds)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!m_dirtyRanges[frameIndex])
        {
            return;
        }

        uint32 rangeStart = m_dirtyRanges[frameIndex].GetStart();
        uint32 rangeEnd = m_dirtyRanges[frameIndex].GetEnd();

        uint32 blockIndex = 0;

        typename LinkedList<typename Base::Block>::Iterator beginIt = Base::m_blocks.Begin();
        typename LinkedList<typename Base::Block>::Iterator endIt = Base::m_blocks.End();

        const uint32 numBlocks = Base::m_numBlocks.Get(MemoryOrder::ACQUIRE);

        for (uint32 blockIndex = 0; blockIndex < numBlocks && beginIt != endIt; ++blockIndex, ++beginIt)
        {
            if (blockIndex < rangeStart / Base::numElementsPerBlock)
            {
                continue;
            }

            if (blockIndex * Base::numElementsPerBlock >= rangeEnd)
            {
                break;
            }

            const uint32 offset = blockIndex * Base::numElementsPerBlock;
            const uint32 count = Base::numElementsPerBlock;

            const uint32 bufferOffset = offset * uint32(sizeof(StructType));
            const uint32 bufferSize = count * uint32(sizeof(StructType));

            GpuBufferBase* buffer = outStagingBuffers.PushBack(StagingBufferPool::GetInstance().AcquireStagingBuffer(frameIndex, bufferOffset, bufferSize));
            Assert(buffer != nullptr && buffer->IsCreated());

            outChunkStarts.EmplaceBack(bufferOffset);
            outChunkEnds.EmplaceBack(bufferOffset + bufferSize);

            // copy to staging buffer
            buffer->Copy(0, bufferSize, beginIt->buffer.GetPointer());
        }

        m_dirtyRanges[frameIndex].Reset();
    }

protected:
    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);

    FixedArray<Range<uint32>, g_framesInFlight> m_dirtyRanges;
};

template <class StructType, GpuBufferType BufferType>
class GpuBufferHolder final : public GpuBufferHolderBase
{
public:
    GpuBufferHolder(uint32 initialCount = 0)
        : GpuBufferHolderBase(TypeWrapper<StructType> {}),
          m_pool(CreateNameFromDynamicString(ANSIString("GfxBuffers_") + TypeNameWithoutNamespace<StructType>().Data()), initialCount)
    {
        GpuBufferHolderBase::CreateBuffers(BufferType, initialCount, sizeof(StructType));
    }

    GpuBufferHolder(const GpuBufferHolder& other) = delete;
    GpuBufferHolder& operator=(const GpuBufferHolder& other) = delete;

    virtual ~GpuBufferHolder() override = default;

    virtual SizeType Count() const override
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

    virtual void UpdateBufferData(FrameBase* frame) override
    {
        const uint32 frameIndex = frame->GetFrameIndex();

        Array<GpuBufferBase*> stagingBuffers;
        Array<uint32> chunkStarts;
        Array<uint32> chunkEnds;

        m_pool.BuildStagingBuffers(frameIndex, stagingBuffers, chunkStarts, chunkEnds);

        // sanity check, ensure that the chunks are in ascending order
        for (SizeType i = 1; i < chunkStarts.Size(); ++i)
        {
            AssertDebug(chunkStarts[i] >= chunkEnds[i - 1]);
        }

        // sanity check, ensure that the chunks are within bounds of our main gpu buffer
        const SizeType gpuBufferSize = m_gpuBuffer->Size();

        for (SizeType i = 0; i < chunkStarts.Size(); ++i)
        {
            AssertDebug(chunkStarts[i] < gpuBufferSize);
            AssertDebug(chunkEnds[i] <= gpuBufferSize);
        }

        GpuBufferHolderBase::CopyToGpuBuffer(frame, stagingBuffers, chunkStarts, chunkEnds);
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

} // namespace hyperion