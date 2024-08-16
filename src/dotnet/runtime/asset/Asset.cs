using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct Asset
    {
        private IntPtr ptr;

        public Asset(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public Node GetNode()
        {
            throw new NotImplementedException();
            // if (!IsValid)
            // {
            //     throw new Exception("Asset is not valid");
            // }

            // ManagedNode node = new ManagedNode();
            // Asset_GetNode(ptr, out node);

            // if (!node.IsValid)
            // {
            //     throw new Exception("Failed to get node from asset");
            // }

            // return new Node(node);
        }

        public Texture GetTexture()
        {
            throw new NotImplementedException();
            // if (!IsValid)
            // {
            //     throw new Exception("Asset is not valid");
            // }

            // ManagedHandle handle = new ManagedHandle();
            // Asset_GetTexture(ptr, out handle);

            // if (!handle.IsValid)
            // {
            //     throw new Exception("Failed to get texture from asset");
            // }

            // return new Texture(handle);
        }

        [DllImport("hyperion", EntryPoint = "Asset_GetNode")]
        private static extern void Asset_GetNode(IntPtr assetPtr, [Out] out ManagedNode node);

        [DllImport("hyperion", EntryPoint = "Asset_GetTexture")]
        private static extern void Asset_GetTexture(IntPtr assetPtr, [Out] out ManagedHandle handle);
    }
}