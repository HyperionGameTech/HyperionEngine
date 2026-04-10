#pragma once

#include "Backend/RxGpuBuffer.hpp"
#include "RxVertex.hpp"

#include <Core/FxAnonArray.hpp>
#include <Core/FxSizedArray.hpp>

class RxVertexList
{
public:
    RxVertexList() = default;

    void CreateSlimFrom(const FxSizedArray<float32>& positions);
    void CreateSlimFrom(const FxSizedArray<FxVec3f>& positions);

    void CreateFrom(const FxSizedArray<FxVec3f>& positions, const FxSizedArray<FxVec3f>& normals,
                    const FxSizedArray<FxVec2f>& uvs, const FxSizedArray<FxVec3f>& tangents,
                    const FxSizedArray<FxVec4f>& bone_weights, const FxSizedArray<FxVec4u>& bone_ids);


    void CreateFrom(const FxSizedArray<float32>& positions, const FxSizedArray<float32>& normals,
                    const FxSizedArray<float32>& uvs, const FxSizedArray<float32>& tangents,
                    const FxSizedArray<float32>& bone_weights, const FxSizedArray<uint32>& bone_ids);

    template <RxVertexType TVertexType>
    void CreateFrom(FxSizedArray<RxVertex<TVertexType>>&& vertices)
    {
        VertexType = TVertexType;
        mLocalBuffer = std::move(vertices);
    }

    template <RxVertexType TVertexType>
    void CreateAsCopyOf(const FxSizedArray<RxVertex<TVertexType>>& vertices)
    {
        VertexType = TVertexType;
        mLocalBuffer.InitAsCopyOf(vertices);
    }

    void UploadToGpu() { GpuBuffer.Create(RxGpuBufferType::eVertexBuffer, mLocalBuffer); }

    /** @brief Returns true if the vertex type supports storing normals */
    FX_FORCE_INLINE bool SupportsNormals() const { return (VertexType != RxVertexType::eSlim); }

    /** @brief Returns true if the vertex buffer has been supplied values for normals. */
    FX_FORCE_INLINE bool HasNormals() const { return bContainsNormals; }

    FX_FORCE_INLINE bool IsSkinned() const { return VertexType == RxVertexType::eSkinned; }

    void DestroyLocalBuffer() { mLocalBuffer.Free(); }
    void Destroy()
    {
        GpuBuffer.Destroy();

        DestroyLocalBuffer();
    }

    FxAnonArray& GetLocalBuffer() { return mLocalBuffer; }
    const FxAnonArray& GetLocalBuffer() const { return mLocalBuffer; }


    ~RxVertexList() { Destroy(); }

public:
    RxVertexType VertexType = RxVertexType::eDefault;
    RxGpuBuffer GpuBuffer {};

    bool bContainsNormals : 1 = false;
    bool bContainsUVs : 1 = false;
    bool bContainsTangents : 1 = false;

private:
    FxAnonArray mLocalBuffer;
};
