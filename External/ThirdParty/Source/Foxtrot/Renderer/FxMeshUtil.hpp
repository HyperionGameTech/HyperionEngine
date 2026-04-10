#pragma once

#include <Core/FxAnonArray.hpp>
#include <Math/FxVec3.hpp>
#include <Renderer/RxVertexList.hpp>
#include <cfloat>

class FxMeshUtil
{
public:
    static FxVec3f CalculateDimensions(const RxVertexList& vertex_list)
    {
        const FxAnonArray& vertices = vertex_list.GetLocalBuffer();

        if (vertices.IsEmpty()) {
            FxLogWarning("Cannot calculate dimensions as there are no vertices!");
            return FxVec3f::sZero;
        }

        FxVec3f min_vertex = FxVec3f(FLT_MAX);
        FxVec3f max_vertex = FxVec3f(-FLT_MAX);

        for (uint32 i = 0; i < vertices.Size; i++) {
            // Since the position resides at the same location for each vertex type (see static_assert in
            // RxVertexUtil::GetPosition), the vertex type used here can be anything, and offsets are preserved as the
            // size of the object is stored in the anonymous buffer.
            FxVec3f position = RxVertexUtil::GetPosition(
                *reinterpret_cast<const RxVertex<RxVertexType::eDefault>*>(vertices.GetRaw(i)));

            min_vertex = FxVec3f::Min(min_vertex, position);
            max_vertex = FxVec3f::Max(max_vertex, position);
        }

        // Return the difference between the min and max positions
        max_vertex -= min_vertex;

        return max_vertex;
    }
};
