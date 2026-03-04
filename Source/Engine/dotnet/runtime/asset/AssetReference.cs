using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetReference")]
    [StructLayout(LayoutKind.Explicit, Size = 16, Pack = 8)]
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