using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name = "AssetReference")]
    [StructLayout(LayoutKind.Explicit)]
    public unsafe struct AssetReference
    {
        [FieldOffset(0)]
        private Handle<AssetObject> handle;

        [FieldOffset(0)]
        private AssetPath assetPath;

        public AssetReference()
        {
        }
    }
}