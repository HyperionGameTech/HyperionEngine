#pragma once

#include "Backend/RxGpuBuffer.hpp"
#include "RxVertexList.hpp"

#include <Math/FxQuat.hpp>

struct FxMeshBone
{
    FxVec3f Position;
    FxQuat Rotation;
};

class FxPrimitiveMesh
{
public:
    FxPrimitiveMesh() = default;

    template <RxVertexType TVertexType>
    FxPrimitiveMesh(FxSizedArray<RxVertex<TVertexType>>& vertices)
    {
        UploadVertices(vertices);
    }

    template <RxVertexType TVertexType>
    FxPrimitiveMesh(FxSizedArray<RxVertex<TVertexType>>& vertices, FxSizedArray<uint32>& indices)
    {
        CreateFromData(vertices, indices);
    }

    /**
     * @brief Recalculates normals if needed and uploads the vertex list to the GPU.
     */
    inline void UploadVertices()
    {
        if (VertexList.SupportsNormals() && !VertexList.HasNormals()) {
            FxLogDebug("Calculating normals for mesh");
            RecalculateNormals();
        }

        VertexList.UploadToGpu();
    }

    /**
     * @brief Uploads the vertices to the mesh's GPU buffers and copies the data into a cpu-side buffer if the property
     * `KeepInMemory` is true.
     */
    template <RxVertexType TVertexType>
    void UploadVertices(const FxSizedArray<RxVertex<TVertexType>>& vertices)
    {
        VertexList.CreateAsCopyOf<TVertexType>(vertices);
        UploadVertices();
    }

    template <RxVertexType TVertexType>
    void UploadVertices(FxSizedArray<RxVertex<TVertexType>>&& vertices)
    {
        VertexList.CreateFrom<TVertexType>(std::move(vertices));
        UploadVertices();
    }

    /** @brief Uploads mesh indices to a primitive mesh. */
    void UploadIndices(const FxSizedArray<uint32>& indices)
    {
        LocalIndexBuffer.InitAsCopyOf(indices);
        GpuIndexBuffer.Create(RxGpuBufferType::eIndexBuffer, FxSlice(indices));
    }

    /**
     * @brief Uploads mesh indices to a primtive mesh, and stores the indices without copy if the property
     * `KeepInMemory` is true.
     */
    void UploadIndices(FxSizedArray<uint32>&& indices)
    {
        GpuIndexBuffer.Create<uint32>(RxGpuBufferType::eIndexBuffer, indices);
        LocalIndexBuffer = std::move(indices);
    }

    RxVertexList& GetVertices()
    {
        if (!bKeepInMemory) {
            FxLogWarning("Requesting vertices from a primitive mesh while `KeepInMemory` != true!");
        }

        // Note that VertexList.LocalBuffer will be empty given bKeepInMemory is false.
        return VertexList;
    }

    FxSizedArray<uint32>& GetIndices()
    {
        if (!bKeepInMemory) {
            FxLogWarning("Requesting indices from a primitive mesh while `KeepInMemory` != true!");
        }

        // This will return an empty array if `KeepInMemory` is false!
        return LocalIndexBuffer;
    }

    bool IsWritable() { return (VertexList.GpuBuffer.Initialized && GpuIndexBuffer.Initialized); }

    RxGpuBuffer& GetVertexBuffer() { return VertexList.GpuBuffer; }
    RxGpuBuffer& GetIndexBuffer() { return GpuIndexBuffer; }

    void Render(const RxCommandBuffer& cmd, uint32 num_instances)
    {
        const VkDeviceSize offset = 0;
        //        RxFrameData* frame = Rx_Fwd_GetFrame();

        vkCmdBindVertexBuffers(cmd.CommandBuffer, 0, 1, &VertexList.GpuBuffer.Buffer, &offset);
        vkCmdBindIndexBuffer(cmd.CommandBuffer, GpuIndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd.CommandBuffer, static_cast<uint32>(GpuIndexBuffer.Size / sizeof(uint32)), num_instances, 0,
                         0, 0);
    }

    void RecalculateNormals()
    {
        using VertexType = RxVertex<RxVertexType::eDefault>;

        if (LocalIndexBuffer.IsEmpty()) {
            FxLogWarning("Cannot recalculate normals as local indices are missing!");
            return;
        }

        FxAnonArray& vertices = VertexList.GetLocalBuffer();

        if (vertices.IsEmpty()) {
            FxLogWarning("Cannot recalculate normals as local vertices are missing!");
            return;
        }

        const uint32 num_vertices = vertices.Size;
        const uint32 num_faces = num_vertices / 3;

        // Zero the mesh normals
        for (uint32 index = 0; index < num_vertices; index++) {
            memset(vertices.Get<VertexType>(index).Normal, 0, sizeof(VertexType::Normal));
        }

        for (uint32 index = 0; index < num_faces; index++) {
            // Indices for each vertex of the triangle
            const uint32 index_a = LocalIndexBuffer.pData[index];
            const uint32 index_b = LocalIndexBuffer.pData[index + 1];
            const uint32 index_c = LocalIndexBuffer.pData[index + 2];

            /*
                        A
                      / |
                    /   |
                  /     |
                B-------C

                Get the edge between A and B and the edge between C and B.
                then, we get the normal using:

                Normal = EdgeA x EdgeB.

                Average across vertices and normalize to remove scale.
             */


            VertexType& vertex_a = vertices.Get<VertexType>(index_a);
            VertexType& vertex_b = vertices.Get<VertexType>(index_b);
            VertexType& vertex_c = vertices.Get<VertexType>(index_c);

            const FxVec3f edge_a = FxVec3f::FromDifference(vertex_a.Position, vertex_b.Position);
            const FxVec3f edge_b = FxVec3f::FromDifference(vertex_c.Position, vertex_b.Position);

            const FxVec3f normal = edge_a.Cross(edge_b);

            vertex_a.Normal[0] += normal.X;
            vertex_a.Normal[1] += normal.Y;
            vertex_a.Normal[2] += normal.Z;

            vertex_b.Normal[0] += normal.X;
            vertex_b.Normal[1] += normal.Y;
            vertex_b.Normal[2] += normal.Z;

            vertex_c.Normal[0] += normal.X;
            vertex_c.Normal[1] += normal.Y;
            vertex_c.Normal[2] += normal.Z;
        }


        for (uint32 index = 0; index < vertices.Size; index++) {
            VertexType& vertex = vertices.Get<VertexType>(index);

            FxVec3f vec(vertex.Position);
            vec.NormalizeIP();
            vec.CopyTo(vertex.Normal);
        }

        VertexList.bContainsNormals = true;
    }

    void Destroy()
    {
        if (bIsReference || !bIsReady.load()) {
            return;
        }

        bIsReady.store(false);

        VertexList.Destroy();

        GpuIndexBuffer.Destroy();
        LocalIndexBuffer.Free();
    }

    ~FxPrimitiveMesh() { Destroy(); }


public:
    RxVertexList VertexList;

    FxSizedArray<FxMeshBone> Bones;

    std::atomic_bool bIsReady = std::atomic_bool(false);

    bool bIsReference = false;
    bool bKeepInMemory = false;

    RxGpuBuffer GpuIndexBuffer;
    FxSizedArray<uint32> LocalIndexBuffer;
};
