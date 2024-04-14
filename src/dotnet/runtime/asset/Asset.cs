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

            ManagedNode node = new ManagedNode();
            Asset_GetNode(ptr, out node);

            if (!node.Valid)
            {
                return null;
            }

            return new Node(node);
        }

        public Texture GetTexture()
        {
            if (!Valid)
            {
                return null;
            }

            ManagedHandle handle = new ManagedHandle();
            Asset_GetTexture(ptr, out handle);

            if (!handle.Valid)
            {
                return null;
            }

            return new Texture(handle);
        }

        [DllImport("hyperion", EntryPoint = "Asset_GetNode")]
        private static extern void Asset_GetNode(IntPtr assetPtr, [Out] out ManagedNode node);

        [DllImport("hyperion", EntryPoint = "Asset_GetTexture")]
        private static extern void Asset_GetTexture(IntPtr assetPtr, [Out] out ManagedHandle handle);
    }
}