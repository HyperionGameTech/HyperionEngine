using System;
using System.IO;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Hyperion;
using System.Reflection.Metadata;

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
                uint count = AssetRegistry_GetPackage(NativeAddress, IntPtr.Zero);
                
                if (count == 0)
                {
                    yield break;
                }

                IntPtr packageHandlePtrs = Marshal.AllocHGlobal(Marshal.SizeOf<Hyperion.Handle<AssetPackage>>() * (int)count);

                try
                {
                    AssetRegistry_GetPackages(NativeAddress, packageHandlePtrs);

                    for (uint i = 0; i < count; i++)
                    {
                        IntPtr currentPtr = IntPtr.Add(packageHandlePtrs, (int)(i * Marshal.SizeOf<Hyperion.Handle<AssetPackage>>()));
                        Hyperion.Handle<AssetPackage> packageHandle = Marshal.PtrToStructure<Hyperion.Handle<AssetPackage>>(currentPtr);
                        AssetPackage? package = packageHandle.GetValue();

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