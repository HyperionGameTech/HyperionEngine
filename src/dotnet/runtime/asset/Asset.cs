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
                throw new Exception("Asset is not valid");
            }

            ManagedNode node = new ManagedNode();
            Asset_GetNode(ptr, out node);

            if (!node.Valid)
            {
                throw new Exception("Failed to get node from asset");
            }

            return new Node(node);
        }

        public Texture GetTexture()
        {
            if (!Valid)
            {
                throw new Exception("Asset is not valid");
            }

            ManagedHandle handle = new ManagedHandle();
            Asset_GetTexture(ptr, out handle);

            if (!handle.Valid)
            {
                throw new Exception("Failed to get texture from asset");
            }

            return new Texture(handle);
        }

        [DllImport("hyperion", EntryPoint = "Asset_GetNode")]
        private static extern void Asset_GetNode(IntPtr assetPtr, [Out] out ManagedNode node);

        [DllImport("hyperion", EntryPoint = "Asset_GetTexture")]
        private static extern void Asset_GetTexture(IntPtr assetPtr, [Out] out ManagedHandle handle);
    }
}