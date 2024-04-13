using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct Asset
    {
        public IntPtr ptr;

        public bool Valid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public Node GetNode()
        {
            if (!Valid)
            {
                return null;
            }

            ManagedNode handle = Asset_GetNode(ptr);

            if (!handle.Valid)
            {
                return null;
            }

            return new Node(handle);
        }

        public Texture GetTexture()
        {
            if (!Valid)
            {
                return null;
            }

            ManagedHandle handle = Asset_GetTexture(ptr);

            if (!handle.Valid)
            {
                return null;
            }

            return new Texture(handle);
        }

        [DllImport("hyperion", EntryPoint = "Asset_GetNode")]
        private static extern ManagedNode Asset_GetNode(IntPtr assetPtr);

        [DllImport("hyperion", EntryPoint = "Asset_GetTexture")]
        private static extern ManagedHandle Asset_GetTexture(IntPtr assetPtr);
    }
}