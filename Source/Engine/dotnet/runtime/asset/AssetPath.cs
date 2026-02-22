using System.Runtime.InteropServices;

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