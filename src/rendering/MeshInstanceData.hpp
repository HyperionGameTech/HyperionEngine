/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MESH_INSTANCE_DATA_HPP
#define HYPERION_MESH_INSTANCE_DATA_HPP

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/Memory.hpp>

#include <core/math/Transform.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Matrix4.hpp>

namespace hyperion {

struct MeshInstanceData;

HYP_API extern void MeshInstanceData_PostLoad(MeshInstanceData&);

HYP_STRUCT(PostLoad = "MeshInstanceData_PostLoad", Size = 104)
struct MeshInstanceData
{
    static constexpr uint32 max_buffers = 8;

    HYP_FIELD(Property = "NumInstances", Serialize = true, Editor = true, Description = "The number of instances of this mesh. This is used to determine how many instances to render in a single draw call. If this is set to 1, the mesh will be rendered as a single instance. If this is greater than 1, the mesh will be rendered as multiple instances.")
    uint32 num_instances = 1;

    HYP_FIELD(Property = "EnableAutoInstancing", Serialize = true, Editor = true, Description = "Enable automatic instancing for this mesh instance data. If enabled, the renderer will automatically batch instances of this mesh together for rendering, regardless of the explicitly set number of instances. This can improve performance by reducing draw calls for duplicate meshes, but may consume more GPU memory if instancing is under utilized for this mesh.")
    bool enable_auto_instancing = false;

    HYP_FIELD(Property = "Buffers", Serialize = true)
    Array<ByteBuffer, DynamicAllocator> buffers;

    HYP_FIELD(Property = "BufferStructSizes", Serialize = true)
    FixedArray<uint32, max_buffers> buffer_struct_sizes;

    HYP_FIELD(Property = "BufferStructAlignments", Serialize = true)
    FixedArray<uint32, max_buffers> buffer_struct_alignments;

    HYP_FORCE_INLINE bool operator==(const MeshInstanceData& other) const
    {
        return num_instances == other.num_instances
            && enable_auto_instancing == other.enable_auto_instancing
            && buffers == other.buffers
            && buffer_struct_sizes == other.buffer_struct_sizes
            && buffer_struct_alignments == other.buffer_struct_alignments;
    }

    HYP_FORCE_INLINE bool operator!=(const MeshInstanceData& other) const
    {
        return num_instances != other.num_instances
            || enable_auto_instancing != other.enable_auto_instancing
            || buffers != other.buffers
            || buffer_struct_sizes != other.buffer_struct_sizes
            || buffer_struct_alignments != other.buffer_struct_alignments;
    }

    HYP_FORCE_INLINE uint32 NumInstances() const
    {
        return num_instances;
    }

    template <class StructType>
    void SetBufferData(int buffer_index, StructType* ptr, SizeType count)
    {
        static_assert(is_pod_type<StructType>, "Struct type must a POD type");

        AssertThrowMsg(buffer_index < max_buffers, "Buffer index %d must be less than maximum number of buffers (%u)", buffer_index, max_buffers);

        if (buffers.Size() <= buffer_index)
        {
            buffers.Resize(buffer_index + 1);
        }

        buffer_struct_sizes[buffer_index] = sizeof(StructType);
        buffer_struct_alignments[buffer_index] = alignof(StructType);

        buffers[buffer_index].SetSize(sizeof(StructType) * count);
        Memory::MemCpy(buffers[buffer_index].Data(), ptr, sizeof(StructType) * count);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(num_instances);
        hc.Add(buffers);
        hc.Add(buffer_struct_sizes);
        hc.Add(buffer_struct_alignments);

        return hc;
    }
};

} // namespace hyperion

#endif