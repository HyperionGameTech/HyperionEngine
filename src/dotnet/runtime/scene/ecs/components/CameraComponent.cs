using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="CameraComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct CameraComponent : IComponent
    {
        [FieldOffset(0)]
        private Handle<Camera> cameraHandle;

        public void Dispose()
        {
            cameraHandle.Dispose();
        }

        public Camera? Camera
        {
            get
            {
                return cameraHandle.GetValue();
            }
            set
            {
                cameraHandle.Dispose();

                if (value == null)
                {
                    cameraHandle = Handle<Camera>.Empty;
                }
                else
                {
                    cameraHandle = new Handle<Camera>(value);
                }
            }
        }
    }
}