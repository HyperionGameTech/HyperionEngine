using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class Camera
    {
        private ManagedHandle handle;

        public Camera()
        {
            this.handle = new ManagedHandle();
            Camera_Create(out this.handle);
        }

        public Camera(ManagedHandle handle)
        {
            this.handle = handle;
            this.handle.IncRef(Camera_GetTypeID());
        }

        ~Camera()
        {
            handle.DecRef(Camera_GetTypeID());
        }

        public ManagedHandle Handle
        {
            get
            {
                return handle;
            }
        }

        public uint ID
        {
            get
            {
                return handle.id;
            }
        }
        
        [DllImport("hyperion", EntryPoint = "Camera_GetTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID Camera_GetTypeID();

        [DllImport("hyperion", EntryPoint = "Camera_Create")]
        private static extern void Camera_Create([Out] out ManagedHandle handle);
    }
}