using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Topology")]
    public enum Topology : uint
    {
        Triangles = 0,
        TriangleFan,
        TriangleStrip,

        Lines,

        Points
    }

    [HypClassBinding(Name="MeshFlags")]
    [Flags]
    public enum MeshFlags : uint
    {
        None = 0,
        ViewIndependent = 0x1,
    }

    [HypClassBinding(Name="Mesh")]
    public class Mesh : HypObject
    {
        public Mesh()
        {
        }
    }
}