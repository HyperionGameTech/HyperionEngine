using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class MeshBuilder
    {
        public static Mesh Quad()
        {
            MeshBuilder_Quad(out BoxedValueInternal boxed);

            return ReadMesh(ref boxed);
        }

        public static Mesh Cube(bool originOnBottom = false)
        {
            MeshBuilder_Cube(originOnBottom, out BoxedValueInternal boxed);

            return ReadMesh(ref boxed);
        }

        public static Mesh Sphere(uint numDivisions = 8)
        {
            MeshBuilder_NormalizedCubeSphere(numDivisions, out BoxedValueInternal boxed);

            return ReadMesh(ref boxed);
        }

        public static Mesh Cylinder(float radius, float height, uint numSegments = 24)
        {
            MeshBuilder_Cylinder(radius, height, numSegments, out BoxedValueInternal boxed);

            return ReadMesh(ref boxed);
        }

        public static Mesh Cone(float radius, float height, uint numSegments = 24)
        {
            MeshBuilder_Cone(radius, height, numSegments, out BoxedValueInternal boxed);

            return ReadMesh(ref boxed);
        }

        public static Mesh Torus(float majorRadius, float minorRadius, uint majorSegments = 32, uint minorSegments = 16)
        {
            MeshBuilder_Torus(majorRadius, minorRadius, majorSegments, minorSegments, out BoxedValueInternal boxed);

            return ReadMesh(ref boxed);
        }

        private static Mesh ReadMesh(ref BoxedValueInternal boxed)
        {
            try
            {
                return boxed.ReadObject<Mesh>() ?? throw new Exception("MeshBuilder returned no mesh");
            }
            finally
            {
                boxed.Dispose();
            }
        }

        [DllImport("hyperion", EntryPoint = "MeshBuilder_Quad")]
        private static extern void MeshBuilder_Quad([Out] out BoxedValueInternal outBoxed);

        [DllImport("hyperion", EntryPoint = "MeshBuilder_Cube")]
        private static extern void MeshBuilder_Cube([MarshalAs(UnmanagedType.I1)] bool originOnBottom, [Out] out BoxedValueInternal outBoxed);

        [DllImport("hyperion", EntryPoint = "MeshBuilder_NormalizedCubeSphere")]
        private static extern void MeshBuilder_NormalizedCubeSphere(uint numDivisions, [Out] out BoxedValueInternal outBoxed);

        [DllImport("hyperion", EntryPoint = "MeshBuilder_Cylinder")]
        private static extern void MeshBuilder_Cylinder(float radius, float height, uint numSegments, [Out] out BoxedValueInternal outBoxed);

        [DllImport("hyperion", EntryPoint = "MeshBuilder_Cone")]
        private static extern void MeshBuilder_Cone(float radius, float height, uint numSegments, [Out] out BoxedValueInternal outBoxed);

        [DllImport("hyperion", EntryPoint = "MeshBuilder_Torus")]
        private static extern void MeshBuilder_Torus(float majorRadius, float minorRadius, uint majorSegments, uint minorSegments, [Out] out BoxedValueInternal outBoxed);
    }
}
