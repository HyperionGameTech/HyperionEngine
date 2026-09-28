/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void MeshBuilder_Quad(BoxedValue* pOutBoxed)
    {
        *pOutBoxed = BoxedValue(MeshBuilder::Quad());
    }

    HYP_EXPORT void MeshBuilder_Cube(int8 originOnBottom, BoxedValue* pOutBoxed)
    {
        *pOutBoxed = BoxedValue(MeshBuilder::Cube(bool(originOnBottom)));
    }

    HYP_EXPORT void MeshBuilder_NormalizedCubeSphere(uint32 numDivisions, BoxedValue* pOutBoxed)
    {
        *pOutBoxed = BoxedValue(MeshBuilder::NormalizedCubeSphere(numDivisions));
    }

    HYP_EXPORT void MeshBuilder_Cylinder(float radius, float height, uint32 numSegments, BoxedValue* pOutBoxed)
    {
        *pOutBoxed = BoxedValue(MeshBuilder::Cylinder(radius, height, numSegments));
    }

    HYP_EXPORT void MeshBuilder_Cone(float radius, float height, uint32 numSegments, BoxedValue* pOutBoxed)
    {
        *pOutBoxed = BoxedValue(MeshBuilder::Cone(radius, height, numSegments));
    }

    HYP_EXPORT void MeshBuilder_Torus(float majorRadius, float minorRadius, uint32 majorSegments, uint32 minorSegments, BoxedValue* pOutBoxed)
    {
        *pOutBoxed = BoxedValue(MeshBuilder::Torus(majorRadius, minorRadius, majorSegments, minorSegments));
    }

} // extern "C"
