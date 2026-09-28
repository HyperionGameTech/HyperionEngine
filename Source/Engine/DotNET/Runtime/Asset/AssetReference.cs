using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetReference")]
    [StructLayout(LayoutKind.Explicit, Size = 24, Pack = 8)]
    public unsafe ref struct AssetReference
    {
        [FieldOffset(0)]
        private fixed byte _raw[24];

        public AssetReference()
        {
            fixed (byte* pRaw = _raw)
            {
                AssetReference_Construct(pRaw);
            }
        }

        [DllImport("hyperion", EntryPoint = "AssetReference_Construct")]
        private static extern void AssetReference_Construct(byte* pOut);

        //public AssetPath Path
        //{
        //    get { return this.GetAssetPath(); }                             // extension
        //    set { this.SetAssetPath(value); }                               // extension
        //}

        //public AssetObject? AssetObject
        //{
        //    get { return this.Resolve(); }                                  // extension
        //    set { this.SetAssetPath(value?.Path ?? AssetPath.Invalid); }    // extension
        //}

        //public bool Valid => this.IsValid();                                // extension
        //public bool Loaded => this.IsLoaded();                              // extension
    }
}
