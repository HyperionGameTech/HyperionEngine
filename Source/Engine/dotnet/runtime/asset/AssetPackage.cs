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

        public IEnumerable<AssetObject> Assets
        {
            get
            {
                uint count = AssetPackage_GetAssets(NativeAddress, IntPtr.Zero);

                if (count == 0)
                {
                    yield break;
                }

                IntPtr assetHandlePtrs = Marshal.AllocHGlobal(Marshal.SizeOf<Hyperion.Handle>() * (int)count);

                try
                {
                    AssetPackage_GetAssets(NativeAddress, assetHandlePtrs);

                    for (uint i = 0; i < count; i++)
                    {
                        IntPtr currentPtr = IntPtr.Add(assetHandlePtrs, (int)(i * Marshal.SizeOf<Hyperion.Handle>()));
                        Hyperion.Handle assetHandle = Marshal.PtrToStructure<Hyperion.Handle>(currentPtr);
                        AssetObject? asset = (AssetObject?)assetHandle.GetValue();

                        if (asset != null)
                        {
                            yield return asset;
                        }
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(assetHandlePtrs);
                }
            }
        }

        public IEnumerable<AssetPackage> Subpackages
        {
            get
            {
                uint count = AssetPackage_GetSubpackages(NativeAddress, IntPtr.Zero);

                if (count == 0)
                {
                    yield break;
                }

                IntPtr subpackageHandlePtrs = Marshal.AllocHGlobal(Marshal.SizeOf<Hyperion.Handle>() * (int)count);

                try
                {
                    AssetPackage_GetSubpackages(NativeAddress, subpackageHandlePtrs);

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

        [DllImport("hyperion", EntryPoint = "AssetPackage_GetAssets")]
        private static extern uint AssetPackage_GetAssets(IntPtr pPackage, IntPtr pOutAssetHandles);

        [DllImport("hyperion", EntryPoint = "AssetPackage_GetSubpackages")]
        private static extern uint AssetPackage_GetSubpackages(IntPtr pPackage, IntPtr pOutSubpackageHandles);
    }
}