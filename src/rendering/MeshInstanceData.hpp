/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/Memory.hpp>

#include <core/math/Transform.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Matrix4.hpp>

#include <core/object/HypObjectFwd.hpp>

namespace hyperion {

struct MeshInstanceData;

extern void MeshInstanceData_OnPostLoad(MeshInstanceData&);

HYP_STRUCT(PostLoad = "MeshInstanceData_OnPostLoad", Size = 104)
struct MeshInstanceData
{
    HYP_STRUCT_BODY(MeshInstanceData);

    static constexpr uint32 maxBuffers = 8;

    HYP_FIELD(Property = "NumInstances", Serialize = true, Editor = true, Description = "The number of instances of this mesh. This is used to determine how many instances to render in a single draw call. If this is set to 1, the mesh will be rendered as a single instance. If this is greater than 1, the mesh will be rendered as multiple instances.")
    uint32 numInstances = 1;

    HYP_FIELD(Property = "EnableAutoInstancing", Serialize = true, Editor = true, Description = "Enable automatic instancing for this mesh instance data. If enabled, the renderer will automatically batch instances of this mesh together for rendering, regardless of the explicitly set number of instances. This can improve performance by reducing draw calls for duplicate meshes, but may consume more GPU memory if instancing is under utilized for this mesh.")
    bool enableAutoInstancing = false;

    HYP_FIELD(Property = "Buffers", Serialize = true)
    Array<ByteBuffer, DynamicAllocator> buffers;

    HYP_FIELD(Property = "BufferStructSizes", Serialize = true)
    FixedArray<uint32, maxBuffers> bufferStructSizes {};

    HYP_FIELD(Property = "BufferStructAlignments", Serialize = true)
    FixedArray<uint32, maxBuffers> bufferStructAlignments {};

    HYP_FORCE_INLINE bool operator==(const MeshInstanceData& other) const
    {
        return numInstances == other.numInstances
            && enableAutoInstancing == other.enableAutoInstancing
            && buffers == other.buffers
            && bufferStructSizes == other.bufferStructSizes
            && bufferStructAlignments == other.bufferStructAlignments;
    }

    HYP_FORCE_INLINE bool operator!=(const MeshInstanceData& other) const
    {
        return numInstances != other.numInstances
            || enableAutoInstancing != other.enableAutoInstancing
            || buffers != other.buffers
            || bufferStructSizes != other.bufferStructSizes
            || bufferStructAlignments != other.bufferStructAlignments;
    }

    HYP_FORCE_INLINE uint32 NumInstances() const
    {
        return numInstances;
    }

    template <class StructType>
    void SetBufferData(int bufferIndex, StructType* ptr, SizeType count)
    {
        static_assert(IsPodTypeV<StructType>, "Struct type must a POD type");

        AssertDebug(bufferIndex < maxBuffers, "Buffer index {} must be less than maximum number of buffers ({})", bufferIndex, maxBuffers);

        if (buffers.Size() <= bufferIndex)
        {
            buffers.Resize(bufferIndex + 1);
        }

        bufferStructSizes[bufferIndex] = sizeof(StructType);
        bufferStructAlignments[bufferIndex] = alignof(StructType);

        buffers[bufferIndex].SetSize(sizeof(StructType) * count);
        Memory::MemCpy(buffers[bufferIndex].Data(), ptr, sizeof(StructType) * count);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(numInstances);
        hc.Add(buffers);
        hc.Add(bufferStructSizes);
        hc.Add(bufferStructAlignments);

        return hc;
    }
};

} // namespace hyperion
