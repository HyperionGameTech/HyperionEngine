using System;
using System.IO;
using System.Runtime.InteropServices;
using Hyperion;

namespace Hyperion
{
    [ClassBinding(Name = "AssetPath")]
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct AssetPath
    {
        private Name* chain;

        public AssetPath()
        {
            chain = null;
        }
    }
}