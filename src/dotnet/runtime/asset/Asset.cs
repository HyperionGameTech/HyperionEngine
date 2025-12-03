using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    internal struct AssetNativeFunctions
    {
        [DllImport("hyperion", EntryPoint = "Asset_Destroy")]
        internal static extern void Asset_Destroy([In] IntPtr pLoadedAsset);

        [DllImport("hyperion", EntryPoint = "Asset_GetHypData")]
        internal static extern void Asset_GetHypData([In] IntPtr pLoadedAsset, [Out] out HypDataBuffer outDataBuffer);
    }

    public class LoadedAsset : IDisposable
    {
        private HypData? _data = null;

        public LoadedAsset(IntPtr pLoadedAsset)
        {
            if (pLoadedAsset != IntPtr.Zero)
            {
                HypDataBuffer dataBuffer;
                AssetNativeFunctions.Asset_GetHypData(pLoadedAsset, out dataBuffer);

                if (dataBuffer.IsNull)
                {
                    dataBuffer.Dispose();
                    return;
                }

                _data = HypData.FromBuffer(dataBuffer);
            }
        }

        public void Dispose()
        {
            _data?.Dispose();
            _data = null;
        }

        public bool IsValid
        {
            get
            {
                return _data != null && !_data.IsNull;
            }
        }

        public object? Value
        {
            get
            {
                if (_data == null)
                {
                    return null;
                }

                return _data.GetValue();
            }
        }
    }

    public class LoadedAsset<T> : IDisposable
    {
        private HypData? _data = null;

        public LoadedAsset(IntPtr pLoadedAsset)
        {
            if (pLoadedAsset != IntPtr.Zero)
            {
                HypDataBuffer dataBuffer;
                AssetNativeFunctions.Asset_GetHypData(pLoadedAsset, out dataBuffer);

                if (dataBuffer.IsNull)
                {
                    dataBuffer.Dispose();
                    return;
                }

                _data = HypData.FromBuffer(dataBuffer);
            }
        }

        public void Dispose()
        {
            _data?.Dispose();
            _data = null;
        }

        public bool IsValid
        {
            get
            {
                return _data != null && !_data.IsNull;
            }
        }

        public T? Value
        {
            get
            {
                if (_data == null)
                {
                    return default(T);
                }

                return (T?)_data.GetValue();
            }
        }
    }
}