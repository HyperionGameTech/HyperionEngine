using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum Topology : uint
    {
        Triangles = 0,
        TriangleFan,
        TriangleStrip,

        Lines,

        Points
    }

    [HypClassBinding(Name="Mesh")]
    public class Mesh : HypObject
    {
        public Mesh()
        {
        }
    }
}