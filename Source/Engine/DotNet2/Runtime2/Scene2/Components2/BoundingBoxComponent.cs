using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="BoundingBoxComponent")]
    [StructLayout(LayoutKind.Sequential, Size = 32)]
    public ref struct BoundingBoxComponent : IComponent
    {
        public static Class Class => Class.GetClass(typeof(BoundingBoxComponent));

        public BoundingBox WorldAABB;

        public void Dispose()
        {
        }
        
        public unsafe IntPtr NativeAddress
        {
            get
            {
                fixed (BoundingBoxComponent* pThis = &this)
                {
                    return (IntPtr)pThis;
                }
            }
        }
    }
}