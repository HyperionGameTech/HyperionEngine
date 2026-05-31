/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/ByteBuffer.hpp>
#include <Core/memory/Memory.hpp>

#include <Core/math/Transform.hpp>
#include <Core/math/BoundingBox.hpp>
#include <Core/math/Mat4f.hpp>

#include <Core/reflection/ObjectFwd.hpp>

#include <Asset/AssetObject.hpp>

namespace Hyperion {

class InstancedMeshData;

extern void InstancedMeshProxy_OnPostLoad(InstancedMeshData&);

HYP_CLASS(PostLoad = "InstancedMeshProxy_OnPostLoad", AssetBucket = "InstancedMeshData")
class InstancedMeshData : public AssetObject
{
    HYP_OBJECT_BODY(InstancedMeshData);

public:
    static constexpr uint32 MaxBuffers = 5;
    static constexpr const char* BufferNames[MaxBuffers] = {
        "IMD0", "IMD1", "IMD2", "IMD3", "IMD4"
    };

    HYP_FIELD(Property = "Buffers")
    FixedArray<BlobDataReference, MaxBuffers> buffers;

    HYP_FIELD(Property = "BufferStructSizes")
    FixedArray<uint32, MaxBuffers> bufferStructSizes {};

    HYP_FIELD(Property = "BufferStructAlignments")
    FixedArray<uint32, MaxBuffers> bufferStructAlignments {};

    InstancedMeshData() = default;

    explicit InstancedMeshData(Name name)
        : AssetObject(name)
    {
    }

    template <class StructType>
    void SetBufferData(int bufferIndex, const StructType* ptr, size_t count)
    {
        static_assert(is_pod_type_v<StructType>, "Struct type must a POD type");
        static_assert(alignof(StructType) <= 16);

        AssertDebug(bufferIndex < MaxBuffers, "Buffer index {} must be less than maximum number of buffers ({})", bufferIndex, MaxBuffers);

        bufferStructSizes[bufferIndex] = sizeof(StructType);
        bufferStructAlignments[bufferIndex] = alignof(StructType);

        BlobDataReference& ref = buffers[bufferIndex];

        if (count == 0)
        {
            if (!ref.readOnly && ref.raw != nullptr)
            {
                FreeBlobData(ref);
            }

            ref = {};

            MarkDirty();

            return;
        }

        if (ref.readOnly || ref.raw == nullptr || ref.size < sizeof(StructType) * count)
        {
            if (!ref.readOnly && ref.raw != nullptr)
            {
                FreeBlobData(ref);
            }

            AllocateBlobData(ref, ptr, sizeof(StructType) * count, 16);
        }

        Memory::Copy(ref.raw, ptr, sizeof(StructType) * count);

        MarkDirty();
    }

protected:
    void PageBlobData() override;
    void UnpageBlobData() override;

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        for (uint32 i = 0; i < uint32(buffers.Size()); i++)
        {
            if (buffers[i].size == 0)
                continue;

            outReferences.EmplaceBack(BufferNames[i], 1, &buffers[i]);
        }
    }
};

} // namespace Hyperion
