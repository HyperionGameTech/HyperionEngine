using System;

namespace Hyperion
{
    [ClassBinding(Name = "CsgBrushShape")]
    public enum CsgBrushShape : byte
    {
        Box = 0,
        Sphere,
        Cylinder
    }

    [ClassBinding(Name = "CsgOperation")]
    public enum CsgOperation : byte
    {
        Union = 0,
        Subtract,
        Intersect
    }

    [ClassBinding(Name = "EditorCsgState")]
    public class EditorCsgState : ObjectBase
    {
    }
}
