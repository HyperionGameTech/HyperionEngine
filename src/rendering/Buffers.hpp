/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/MemoryPool.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/Range.hpp>

#include <core/Defines.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuBuffer.hpp>

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

struct PendingGpuBufferUpdate
{
    static constexpr uint32 s_bufferSize = 65535; // @TODO Figure out the most efficient size

    uint32 offset = 0;
    uint32 count = 0;
    GpuBufferRef stagingBuffer;

    PendingGpuBufferUpdate();
    PendingGpuBufferUpdate(const PendingGpuBufferUpdate&) = delete;
    PendingGpuBufferUpdate& operator=(const PendingGpuBufferUpdate&) = delete;

    PendingGpuBufferUpdate(PendingGpuBufferUpdate&& other) noexcept;
    PendingGpuBufferUpdate& operator=(PendingGpuBufferUpdate&& other) noexcept;

    ~PendingGpuBufferUpdate();

    void Init(uint32 alignment);
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

    void ApplyPendingUpdates(FrameBase* frame);

protected:
    void CreateBuffers(GpuBufferType type, SizeType count, SizeType size);

    virtual void WriteBufferData_Internal(uint32 index, const void* ptr) = 0;

    TypeId m_structTypeId;
    SizeType m_structSize;

    GpuBufferRef m_gpuBuffer;
    Array<PendingGpuBufferUpdate> m_pendingUpdates;
};

template <class StructType>
class GpuBufferHolderMemoryPool final : public MemoryPool<StructType>
{
public:
    using Base = MemoryPool<StructType>;

    GpuBufferHolderMemoryPool(Name poolName, uint32 initialCount = Base::InitInfo::numInitialElements)
        : Base(poolName, initialCount, /* createInitialBlocks */ true, /* blockInitCtx */ nullptr)
    {
    }

protected:
    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

template <class StructType, GpuBufferType BufferType>
class GpuBufferHolder final : public GpuBufferHolderBase
{
public:
    explicit GpuBufferHolder(uint32 initialCount = 0)
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

    HYP_FORCE_INLINE uint32 AcquireIndex(StructType** outElementPtr)
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_pool.AcquireIndex(outElementPtr);
    }

    virtual uint32 AcquireIndex(void** outElementPtr = nullptr) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

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
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_pool.ReleaseIndex(batchIndex);
    }

    virtual void EnsureCapacity(uint32 index) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        m_pool.EnsureCapacity(index);
    }

    HYP_FORCE_INLINE void WriteBufferData(uint32 index, const StructType& value)
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        m_pool.SetElement(index, value);

        PendingGpuBufferUpdate* update = nullptr;
        // find an existing update that contains this index

        const SizeType indexTimesSize = index * sizeof(StructType);

        for (PendingGpuBufferUpdate& it : m_pendingUpdates)
        {
            if (indexTimesSize >= it.offset && indexTimesSize < it.offset + PendingGpuBufferUpdate::s_bufferSize)
            {
                update = &it;
                break;
            }
        }

        if (update != nullptr)
        {
            update->offset = MathUtil::Min(update->offset, indexTimesSize);
            update->count = MathUtil::Max(update->count, (indexTimesSize + sizeof(StructType)) - update->offset);

            // write to the staging buffer
            AssertDebug(update->stagingBuffer != nullptr);

            update->stagingBuffer->Copy(indexTimesSize % PendingGpuBufferUpdate::s_bufferSize, sizeof(StructType), &value);

            return;
        }

        // make a new update
        PendingGpuBufferUpdate& newUpdate = m_pendingUpdates.EmplaceBack();
        newUpdate.offset = indexTimesSize; // dst offset in the gpu buffer
        newUpdate.count = sizeof(StructType);

        newUpdate.Init(alignof(StructType));

        // write to the staging buffer at the relative offset
        const SizeType relativeOffset = indexTimesSize % PendingGpuBufferUpdate::s_bufferSize;

        newUpdate.stagingBuffer->Copy(relativeOffset, sizeof(StructType), &value);
    }

    virtual void* GetCpuMapping(uint32 index) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        AssertDebug(index < m_pool.NumAllocatedElements(), "Index out of bounds! Index = {}, Size = {}", index, m_pool.NumAllocatedElements());

        return &m_pool.GetElement(index);
    }

private:
    virtual void WriteBufferData_Internal(uint32 index, const void* bufferDataPtr) override
    {
        Threads::AssertOnThread(g_renderThread);
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        WriteBufferData(index, *reinterpret_cast<const StructType*>(bufferDataPtr));
    }

    GpuBufferHolderMemoryPool<StructType> m_pool;
};

} // namespace hyperion
