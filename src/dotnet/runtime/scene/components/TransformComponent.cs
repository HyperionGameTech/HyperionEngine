using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="TransformComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 112)]
    public ref struct TransformComponent : IComponent
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
            get => transform;
            set => transform = value;
        }

        public static Class Class => Class.GetClass(typeof(TransformComponent));
    }
}