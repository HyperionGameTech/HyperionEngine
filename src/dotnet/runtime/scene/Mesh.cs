using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "Topology")]
    public enum Topology : uint
    {
        Triangles = 0,
        TriangleFan,
        TriangleStrip,

        Lines,

        Points
    }

    [ClassBinding(Name = "MeshFlags")]
    [Flags]
    public enum MeshFlags : uint
    {
        None = 0,
        ViewIndependent = 0x1,
    }

    [ClassBinding(Name = "Mesh")]
    public class Mesh : ObjectBase
    {
        public Mesh()
        {
        }
    }
}