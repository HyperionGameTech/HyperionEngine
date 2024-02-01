using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class Mesh : IDisposable
    {
        private ManagedHandle handle;

        public Mesh()
        {
            handle = Mesh_Create();
        }

        public Mesh(ManagedHandle handle)
        {
            this.handle = handle;
        }

        public void Dispose()
        {
            handle.Dispose();
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

        [DllImport("libhyperion", EntryPoint = "Mesh_Create")]
        private static extern ManagedHandle Mesh_Create();
    }
}