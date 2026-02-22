using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetRegistry")]
    public class AssetRegistry : ObjectBase
    {
        public AssetRegistry()
        {
        }

        public IEnumerable<AssetPackage> Packages
        {
            get
            {
                uint count = AssetRegistry_GetPackages(NativeAddress, IntPtr.Zero);
                
                if (count == 0)
                {
                    yield break;
                }

                IntPtr packageHandlePtrs = Marshal.AllocHGlobal(Marshal.SizeOf<Hyperion.Handle>() * (int)count);

                try
                {
                    AssetRegistry_GetPackages(NativeAddress, packageHandlePtrs);

                    for (uint i = 0; i < count; i++)
                    {
                        IntPtr currentPtr = IntPtr.Add(packageHandlePtrs, (int)(i * Marshal.SizeOf<Hyperion.Handle>()));
                        Hyperion.Handle packageHandle = Marshal.PtrToStructure<Hyperion.Handle>(currentPtr);
                        AssetPackage? package = (AssetPackage?)packageHandle.GetValue();

                        if (package != null)
                        {
                            yield return package;
                        }
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(packageHandlePtrs);
                }
            }
        }

        [DllImport("hyperion", EntryPoint = "AssetRegistry_GetPackages")]
        private static extern uint AssetRegistry_GetPackages(IntPtr pRegistry, IntPtr pOutPackageHandles);
    }
}