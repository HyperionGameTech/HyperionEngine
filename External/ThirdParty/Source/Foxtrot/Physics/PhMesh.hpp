#pragma once

#include <ThirdParty/Jolt/Jolt.h>
// Jolt headers
#include <ThirdParty/Jolt/Geometry/IndexedTriangle.h>
#include <ThirdParty/Jolt/Math/Float3.h>
#include <ThirdParty/Jolt/Physics/Collision/Shape/MeshShape.h>
#include <ThirdParty/Jolt/Physics/Collision/Shape/Shape.h>

#include <Core/FxMemberRef.hpp>
#include <Core/FxSizedArray.hpp>
#include <Renderer/FxPrimitiveMesh.hpp>

class PhMesh
{
public:
    PhMesh(const FxPrimitiveMesh& mesh);

    JPH::MeshShapeSettings GetShapeSettings() const;

public:
    JPH::VertexList VertexList;
    JPH::IndexedTriangleList TriangleList;
};
