/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Rendering/Util/MeshBoolean.hpp>
#include <Rendering/Util/MeshBuilder.hpp>
#include <Rendering/Mesh.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Utilities/Format.hpp>

namespace Hyperion {
namespace tests {
namespace rendering {

namespace {

int g_passCount = 0;
int g_failCount = 0;

void Check(const char* testName, bool condition, const String& detail = "")
{
    if (condition)
    {
        ++g_passCount;
        HYP_LOG(Engine, Info, "[PASS] {}", testName);
    }
    else
    {
        ++g_failCount;
        HYP_LOG(Engine, Error, "[FAIL] {} {}", testName, detail);
    }
}

Mat4f MakeTransform(const Vec3f& translation, const Vec3f& scale)
{
    return Mat4f::Translation(translation) * Mat4f::Scaling(scale);
}

double SignedVolume(const MeshBooleanResult& result)
{
    const size_t stride = result.meshDesc.meshAttributes.inputLayout.VertexSize() / sizeof(float);

    double volume = 0.0;

    for (size_t triangleStart = 0; triangleStart + 2 < result.indices.Size(); triangleStart += 3)
    {
        const float* p0 = result.vertexData.Data() + result.indices[triangleStart + 0] * stride;
        const float* p1 = result.vertexData.Data() + result.indices[triangleStart + 1] * stride;
        const float* p2 = result.vertexData.Data() + result.indices[triangleStart + 2] * stride;

        volume += double(p0[0]) * (double(p1[1]) * double(p2[2]) - double(p1[2]) * double(p2[1]))
            - double(p0[1]) * (double(p1[0]) * double(p2[2]) - double(p1[2]) * double(p2[0]))
            + double(p0[2]) * (double(p1[0]) * double(p2[1]) - double(p1[1]) * double(p2[0]));
    }

    return volume / 6.0;
}

Handle<Mesh> MakeMeshFromResult(const MeshBooleanResult& result)
{
    VertexArrayView vertices {};
    vertices.floatData = result.vertexData.Data();
    vertices.vertexCount = result.meshDesc.lods[0].numVertices;
    vertices.layoutDesc = result.meshDesc.meshAttributes.inputLayout;

    MeshDataView meshData {};
    meshData.vertices[0] = vertices;
    meshData.indices[0] = result.indices.ToByteView();

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetMeshData(result.meshDesc, meshData);

    return mesh;
}

bool AreNormalsAxisAligned(const MeshBooleanResult& result)
{
    const size_t stride = result.meshDesc.meshAttributes.inputLayout.VertexSize() / sizeof(float);

    for (uint32 vertexIndex = 0; vertexIndex < result.meshDesc.lods[0].numVertices; vertexIndex++)
    {
        const float* normal = result.vertexData.Data() + vertexIndex * stride + 3;

        const float largestComponent = MathUtil::Max(MathUtil::Abs(normal[0]), MathUtil::Max(MathUtil::Abs(normal[1]), MathUtil::Abs(normal[2])));
        const float lengthSquared = normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];

        if (MathUtil::Abs(largestComponent - 1.0f) > 0.001f || MathUtil::Abs(lengthSquared - 1.0f) > 0.001f)
        {
            return false;
        }
    }

    return true;
}

void CheckVolume(const char* testName, const MeshBooleanResult& result, double expectedVolume, double tolerance)
{
    if (result.error != MeshBooleanError::None)
    {
        Check(testName, false, HYP_FORMAT("error: {}", MeshBoolean::GetErrorMessage(result.error)));

        return;
    }

    const double volume = SignedVolume(result);

    Check(testName, MathUtil::Abs(-volume - expectedVolume) <= tolerance,
        HYP_FORMAT("volume {} expected {}", -volume, expectedVolume));
}

void CheckClosed(const char* testName, const MeshBooleanResult& result)
{
    if (result.error != MeshBooleanError::None)
    {
        Check(testName, false, HYP_FORMAT("error: {}", MeshBoolean::GetErrorMessage(result.error)));

        return;
    }

    Handle<Mesh> mesh = MakeMeshFromResult(result);

    Check(testName, MeshBoolean::ValidateSolid(mesh.Get()) == MeshBooleanError::None);
}

void TestPrimitivesAreSolid()
{
    Handle<Mesh> cube = MeshBuilder::Cube();
    Handle<Mesh> sphere = MeshBuilder::NormalizedCubeSphere(8);
    Handle<Mesh> cylinder = MeshBuilder::Cylinder(1.0f, 2.0f, 24);
    Handle<Mesh> quad = MeshBuilder::Quad();

    Check("Primitives: cube is a closed solid", MeshBoolean::ValidateSolid(cube.Get()) == MeshBooleanError::None);
    Check("Primitives: cube sphere is a closed solid", MeshBoolean::ValidateSolid(sphere.Get()) == MeshBooleanError::None);
    Check("Primitives: cylinder is a closed solid", MeshBoolean::ValidateSolid(cylinder.Get()) == MeshBooleanError::None);
    Check("Primitives: quad is rejected as open", MeshBoolean::ValidateSolid(quad.Get()) == MeshBooleanError::TargetNotClosed);
}

void TestSubtractCorner()
{
    Handle<Mesh> cube = MeshBuilder::Cube();

    const MeshBooleanResult result = MeshBoolean::Apply(
        MeshBooleanOperand { cube.Get() },
        MeshBooleanOperand { cube.Get(), MakeTransform(Vec3f(1.0f), Vec3f(0.5f)) },
        MeshBooleanOperation::Subtract);

    CheckVolume("Subtract: corner notch removes an eighth of the brush", result, 8.0 - 0.125, 0.001);
    CheckClosed("Subtract: corner notch stays closed", result);
    Check("Subtract: hard edges keep flat axis-aligned normals", result.error == MeshBooleanError::None && AreNormalsAxisAligned(result));
}

void TestSubtractCoplanar()
{
    Handle<Mesh> cube = MeshBuilder::Cube();

    const MeshBooleanResult result = MeshBoolean::Apply(
        MeshBooleanOperand { cube.Get() },
        MeshBooleanOperand { cube.Get(), MakeTransform(Vec3f(0.0f, 0.5f, 0.0f), Vec3f(0.5f)) },
        MeshBooleanOperation::Subtract);

    CheckVolume("Subtract: brush flush with the top face carves a notch", result, 7.0, 0.001);
    CheckClosed("Subtract: flush brush result stays closed", result);
}

void TestUnion()
{
    Handle<Mesh> cube = MeshBuilder::Cube();

    const MeshBooleanResult result = MeshBoolean::Apply(
        MeshBooleanOperand { cube.Get() },
        MeshBooleanOperand { cube.Get(), MakeTransform(Vec3f(1.0f, 0.0f, 0.0f), Vec3f(1.0f)) },
        MeshBooleanOperation::Union);

    CheckVolume("Union: overlapping cubes", result, 12.0, 0.001);
    CheckClosed("Union: result stays closed", result);
}

void TestIntersectCylinder()
{
    Handle<Mesh> cube = MeshBuilder::Cube();
    Handle<Mesh> cylinder = MeshBuilder::Cylinder(1.0f, 2.0f, 24);

    const double polygonArea = 0.5 * 24.0 * MathUtil::Sin(2.0 * MathUtil::pi<double> / 24.0);

    const MeshBooleanResult result = MeshBoolean::Apply(
        MeshBooleanOperand { cube.Get() },
        MeshBooleanOperand { cylinder.Get(), MakeTransform(Vec3f(0.0f), Vec3f(1.0f, 4.0f, 1.0f)) },
        MeshBooleanOperation::Intersect);

    CheckVolume("Intersect: cube trims a tall cylinder to its height", result, polygonArea * 2.0, 0.001);
    CheckClosed("Intersect: result stays closed", result);
}

void TestSubtractSphereThrough()
{
    Handle<Mesh> cube = MeshBuilder::Cube();
    Handle<Mesh> sphere = MeshBuilder::NormalizedCubeSphere(8);

    const MeshBooleanResult result = MeshBoolean::Apply(
        MeshBooleanOperand { cube.Get() },
        MeshBooleanOperand { sphere.Get(), MakeTransform(Vec3f(0.0f, 0.0f, 1.0f), Vec3f(0.5f)) },
        MeshBooleanOperation::Subtract);

    const double hemisphereVolume = (2.0 / 3.0) * MathUtil::pi<double> * 0.125;

    CheckVolume("Subtract: sphere on a face carves roughly a hemisphere", result, 8.0 - hemisphereVolume, 0.02);
    CheckClosed("Subtract: sphere carve stays closed", result);
}

void TestSubtractEverything()
{
    Handle<Mesh> cube = MeshBuilder::Cube();

    const MeshBooleanResult result = MeshBoolean::Apply(
        MeshBooleanOperand { cube.Get() },
        MeshBooleanOperand { cube.Get(), MakeTransform(Vec3f(0.0f), Vec3f(2.0f)) },
        MeshBooleanOperation::Subtract);

    Check("Subtract: swallowing the target reports an empty result", result.error == MeshBooleanError::EmptyResult);
}

} // namespace

ENGINE_API void RunMeshBooleanTests()
{
    g_passCount = 0;
    g_failCount = 0;

    if (!MeshBoolean::IsSupported())
    {
        HYP_LOG(Engine, Warning, "Mesh boolean tests skipped - built without manifold");

        return;
    }

    TestPrimitivesAreSolid();
    TestSubtractCorner();
    TestSubtractCoplanar();
    TestUnion();
    TestIntersectCylinder();
    TestSubtractSphereThrough();
    TestSubtractEverything();

    HYP_LOG(Engine, Info, "========== Mesh Boolean Test Results: {} passed, {} failed ==========", g_passCount, g_failCount);

    if (g_failCount > 0)
    {
        HYP_LOG(Engine, Error, "!!! MESH BOOLEAN TEST HAD FAILURES !!!");
    }
}

} // namespace rendering
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
