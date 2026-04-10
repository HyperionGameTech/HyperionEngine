#include "RxVertexList.hpp"

template <typename TType, uint32 TNumComponents>
FX_FORCE_INLINE void WriteOrZero(TType* dst, const FxSizedArray<TType>& src, uint64 index, bool write_or_zero)
{
    constexpr uint32 size = TNumComponents * sizeof(TType);

    if (write_or_zero) {
        memcpy(dst, &src[index * TNumComponents], size);
        return;
    }

    memset(dst, 0, size);
}

template <typename TType, typename TVecType, uint32 TNumComponents>
FX_FORCE_INLINE void WriteOrZeroVec(TType* dst, const FxSizedArray<TVecType>& src, uint64 index, bool write_or_zero)
{
    constexpr uint32 size = TNumComponents * sizeof(TType);

    if (write_or_zero) {
        memcpy(dst, &src[index].mData, size);
        return;
    }

    memset(dst, 0, size);
}


void RxVertexList::CreateFrom(const FxSizedArray<FxVec3f>& positions, const FxSizedArray<FxVec3f>& normals,
                              const FxSizedArray<FxVec2f>& uvs, const FxSizedArray<FxVec3f>& tangents,
                              const FxSizedArray<FxVec4f>& bone_weights, const FxSizedArray<FxVec4u>& bone_ids)
{
    FxAssert(mLocalBuffer.IsEmpty());

    VertexType = RxVertexType::eSlim;

    // Assume default vertex is requested if there are non-null values passed in for the additional components
    if (normals.IsInited() || uvs.IsInited() || tangents.IsInited()) {
        VertexType = RxVertexType::eDefault;
    }

    // Only switch to the skinned vertex if there is animation data passed in. Otherwise, the caller should fallback to
    // the default vertex pipelines.
    if (bone_weights.IsNotEmpty() && bone_ids.IsNotEmpty()) {
        VertexType = RxVertexType::eSkinned;
    }

    const uint32 vertex_size = RxVertexUtil::GetSize(VertexType);

    mLocalBuffer.Create(vertex_size, positions.Size);

    const bool supports_default = (VertexType != RxVertexType::eSlim);
    const bool supports_skinning = (VertexType == RxVertexType::eSkinned);

    bContainsNormals = normals.IsNotEmpty();
    bContainsUVs = uvs.IsNotEmpty();
    bContainsTangents = tangents.IsNotEmpty();

    for (uint64 vertex_index = 0; vertex_index < mLocalBuffer.Capacity; vertex_index++) {
        RxVertex<RxVertexLargestType> vertex;

        memcpy(vertex.Position, &positions[vertex_index].mData, sizeof(vertex.Position));

        // Write the components for a default vertex if the type supports it
        if (supports_default) {
            WriteOrZeroVec<float32, FxVec3f, 3>(vertex.Normal, normals, vertex_index, bContainsNormals);
            WriteOrZeroVec<float32, FxVec2f, 2>(vertex.UV, uvs, vertex_index, bContainsUVs);
            WriteOrZeroVec<float32, FxVec3f, 3>(vertex.Tangent, tangents, vertex_index, bContainsTangents);

            if (supports_skinning) {
                // To support skinned vertices, we enforce that there is data available above; this means we dont need
                // to check in WriteOrZero.

                WriteOrZeroVec<uint32, FxVec4u, 4>(vertex.BoneIds, bone_ids, vertex_index, true);
                WriteOrZeroVec<float32, FxVec4f, 4>(vertex.BoneWeights, bone_weights, vertex_index, true);
            }
        }

        mLocalBuffer.InsertRaw(&vertex);
    }
}

void RxVertexList::CreateFrom(const FxSizedArray<float32>& positions, const FxSizedArray<float32>& normals,
                              const FxSizedArray<float32>& uvs, const FxSizedArray<float32>& tangents,
                              const FxSizedArray<float32>& bone_weights, const FxSizedArray<uint32>& bone_ids)
{
    FxAssert(mLocalBuffer.IsEmpty());
    FxAssert(positions.Size > 0);
    FxAssert(((positions.Size) % 3) == 0);

    VertexType = RxVertexType::eSlim;

    // Assume default vertex is requested if there are non-null values passed in for the additional components
    if (normals.IsInited() || uvs.IsInited() || tangents.IsInited()) {
        VertexType = RxVertexType::eDefault;
    }

    // Only switch to the skinned vertex if there is animation data passed in. Otherwise, the caller should fallback to
    // the default vertex pipelines.
    if (bone_weights.IsNotEmpty() && bone_ids.IsNotEmpty()) {
        VertexType = RxVertexType::eSkinned;
    }

    const uint32 vertex_size = RxVertexUtil::GetSize(VertexType);
    const uint32 amount_of_vertices = positions.Size / 3;

    mLocalBuffer.Create(vertex_size, amount_of_vertices);

    const bool supports_default = (VertexType != RxVertexType::eSlim);
    const bool supports_skinning = (VertexType == RxVertexType::eSkinned);

    bContainsNormals = normals.IsNotEmpty();
    bContainsUVs = uvs.IsNotEmpty();
    bContainsTangents = tangents.IsNotEmpty();

    for (uint64 vertex_index = 0; vertex_index < mLocalBuffer.Capacity; vertex_index++) {
        RxVertex<RxVertexLargestType> vertex;

        memcpy(vertex.Position, &positions[vertex_index * 3], sizeof(vertex.Position));

        // Write the components for a default vertex if the type supports it
        if (supports_default) {
            WriteOrZero<float32, 3>(vertex.Normal, normals, vertex_index, bContainsNormals);
            WriteOrZero<float32, 2>(vertex.UV, uvs, vertex_index, bContainsUVs);
            WriteOrZero<float32, 3>(vertex.Tangent, tangents, vertex_index, bContainsTangents);


            if (supports_skinning) {
                // To support skinned vertices, we enforce that there is data available above; this means we dont need
                // to check in WriteOrZero.
                WriteOrZero<uint32, 4>(vertex.BoneIds, bone_ids, vertex_index, true);
                WriteOrZero<float32, 4>(vertex.BoneWeights, bone_weights, vertex_index, true);
            }
        }

        mLocalBuffer.InsertRaw(&vertex);
    }
}


void RxVertexList::CreateSlimFrom(const FxSizedArray<float32>& positions)
{
    FxAssert(positions.Size > 0);
    FxAssert((positions.Size % 3) == 0);

    VertexType = RxVertexType::eSlim;

    mLocalBuffer.Create(sizeof(RxVertex<RxVertexType::eSlim>), positions.Size / 3);

    for (int i = 0; i < mLocalBuffer.Capacity; i++) {
        RxVertex<RxVertexType::eSlim> vertex;
        memcpy(&vertex.Position, &positions.pData[i * 3], sizeof(float32) * 3);

        mLocalBuffer.Insert(vertex);
    }
}

void RxVertexList::CreateSlimFrom(const FxSizedArray<FxVec3f>& positions)
{
    FxAssert(positions.Size > 0);

    VertexType = RxVertexType::eSlim;

    mLocalBuffer.Create(sizeof(RxVertex<RxVertexType::eSlim>), positions.Size);

    for (int i = 0; i < mLocalBuffer.Capacity; i++) {
        RxVertex<RxVertexType::eSlim> vertex;
        memcpy(&vertex.Position, &positions.pData[i].mData, sizeof(float32) * 3);

        mLocalBuffer.Insert(vertex);
    }
}
