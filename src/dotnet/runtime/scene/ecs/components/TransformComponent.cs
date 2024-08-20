using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="TransformComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 112)]
    public struct TransformComponent : IComponent
    {
        [FieldOffset(0)]
        public Transform transform;
    }
}