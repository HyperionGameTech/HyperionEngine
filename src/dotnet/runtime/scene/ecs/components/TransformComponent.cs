using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="TransformComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 112)]
    public struct TransformComponent : IComponent
    {
        [FieldOffset(0)]
        private Transform transform = Transform.Identity;

        public TransformComponent()
        {
        }

        public void Dispose()
        {
        }

        public Transform Transform
        {
            get
            {
                return transform;
            }
            set
            {
                transform = value;
            }
        }
    }
}