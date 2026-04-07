using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetPackageFlags")]
    [Flags]
    public enum AssetPackageFlags : uint
    {
        None = 0x0,
        Transient = 0x1,
        Hidden = 0x2
    }

    [ClassBinding(Name = "AssetPackage")]
    public class AssetPackage : ObjectBase
    {
        private static readonly LogChannel _logChannel = LogChannel.ByName("Asset");

        public AssetPackage()
        {
        }

        public IEnumerable<AssetDesc> AssetDescs
        {
            get
            {
                uint count = AssetPackage_GetAssetDescs(NativeAddress, IntPtr.Zero, 0);

                if (count == 0)
                {
                    yield break;
                }

                Debug.Assert(Marshal.SizeOf<AssetDesc>() == 12);
                IntPtr assetDescPtrs = Marshal.AllocHGlobal(Marshal.SizeOf<AssetDesc>() * (int)count);

                try
                {
                    AssetPackage_GetAssetDescs(NativeAddress, assetDescPtrs, count);

                    for (uint i = 0; i < count; i++)
                    {
                        IntPtr currentPtr = IntPtr.Add(assetDescPtrs, (int)(i * Marshal.SizeOf<AssetDesc>()));
                        AssetDesc assetDesc = Marshal.PtrToStructure<AssetDesc>(currentPtr);

                        yield return assetDesc;
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(assetDescPtrs);
                }
            }
        }

        public IEnumerable<AssetPackage> Subpackages
        {
            get
            {
                uint count = AssetPackage_GetSubpackages(NativeAddress, IntPtr.Zero, 0);

                if (count == 0)
                {
                    yield break;
                }

                IntPtr subpackageHandlePtrs = Marshal.AllocHGlobal(Marshal.SizeOf<Hyperion.Handle>() * (int)count);

                try
                {
                    AssetPackage_GetSubpackages(NativeAddress, subpackageHandlePtrs, count);

                    for (uint i = 0; i < count; i++)
                    {
                        IntPtr currentPtr = IntPtr.Add(subpackageHandlePtrs, (int)(i * Marshal.SizeOf<Hyperion.Handle>()));
                        Hyperion.Handle subpackageHandle = Marshal.PtrToStructure<Hyperion.Handle>(currentPtr);
                        AssetPackage? subpackage = (AssetPackage?)subpackageHandle.GetValue();

                        if (subpackage != null)
                        {
                            yield return subpackage;
                        }
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(subpackageHandlePtrs);
                }
            }
        }

        public Name Name => this.GetName(); // extension method
        public AssetPackageFlags Flags => (AssetPackageFlags)this.GetFlags(); // extension method
        public bool Hidden => (Flags & AssetPackageFlags.Hidden) != 0;
        public bool Transient => (Flags & AssetPackageFlags.Transient) != 0;

        [DllImport("hyperion", EntryPoint = "AssetPackage_GetAssetDescs")]
        private static extern uint AssetPackage_GetAssetDescs(IntPtr pPackage, IntPtr pOutAssetDescs, uint maxCount);

        [DllImport("hyperion", EntryPoint = "AssetPackage_GetSubpackages")]
        private static extern uint AssetPackage_GetSubpackages(IntPtr pPackage, IntPtr pOutSubpackageHandles, uint maxCount);
    }
}