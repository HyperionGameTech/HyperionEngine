using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    internal struct AssetNativeFunctions
    {
        [DllImport("hyperion", EntryPoint = "Asset_Destroy")]
        internal static extern void Asset_Destroy([In] IntPtr assetPtr);

        [DllImport("hyperion", EntryPoint = "Asset_GetHypData")]
        internal static extern void Asset_GetHypData([In] IntPtr assetPtr, [Out] out HypDataBuffer outHypDataBuffer);
    }

    public class Asset : IDisposable
    {
        private HypData? hypData = null;

        public Asset(IntPtr assetPtr)
        {
            if (assetPtr != IntPtr.Zero)
            {
                HypDataBuffer hypDataBuffer;
                AssetNativeFunctions.Asset_GetHypData(assetPtr, out hypDataBuffer);

                if (hypDataBuffer.IsNull)
                {
                    hypDataBuffer.Dispose();
                    return;
                }

                hypData = HypData.FromBuffer(hypDataBuffer);
            }
        }

        public void Dispose()
        {
            if (hypData != null)
            {
                ((HypData)hypData).Dispose();
                hypData = null;
            }
        }

        public bool IsValid
        {
            get
            {
                return hypData != null && !((HypData)hypData).IsNull;
            }
        }

        public object? Value
        {
            get
            {
                if (hypData == null)
                {
                    return null;
                }

                return ((HypData)hypData).GetValue();
            }
        }
    }

    public class Asset<T> : IDisposable
    {
        private HypData? hypData = null;

        public Asset(IntPtr assetPtr)
        {
            if (assetPtr != IntPtr.Zero)
            {
                HypDataBuffer hypDataBuffer;
                AssetNativeFunctions.Asset_GetHypData(assetPtr, out hypDataBuffer);

                if (hypDataBuffer.IsNull)
                {
                    hypDataBuffer.Dispose();
                    return;
                }

                hypData = HypData.FromBuffer(hypDataBuffer);
            }
        }

        public void Dispose()
        {
            if (hypData != null)
            {
                ((HypData)hypData).Dispose();
                hypData = null;
            }
        }

        public bool IsValid
        {
            get
            {
                return hypData != null && !((HypData)hypData).IsNull;
            }
        }

        public T? Value
        {
            get
            {
                if (hypData == null)
                {
                    return default(T);
                }

                return (T?)((HypData)hypData).GetValue();
            }
        }
    }
}