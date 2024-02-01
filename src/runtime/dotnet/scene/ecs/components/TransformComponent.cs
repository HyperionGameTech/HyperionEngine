using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential)]
    public struct TransformComponent : IComponent
    {
        public Transform transform;
    }
}