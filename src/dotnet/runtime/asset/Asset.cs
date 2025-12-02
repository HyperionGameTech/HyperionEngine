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
        private HypData? m_data = null;

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

                m_data = HypData.FromBuffer(dataBuffer);
            }
        }

        public void Dispose()
        {
            if (m_data != null)
            {
                m_data.Dispose();
                m_data = null;
            }
        }

        public bool IsValid
        {
            get
            {
                return m_data != null && !m_data.IsNull;
            }
        }

        public object? Value
        {
            get
            {
                if (m_data == null)
                {
                    return null;
                }

                return m_data.GetValue();
            }
        }
    }

    public class LoadedAsset<T> : IDisposable
    {
        private HypData? m_data = null;

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

                m_data = HypData.FromBuffer(dataBuffer);
            }
        }

        public void Dispose()
        {
            if (m_data != null)
            {
                m_data.Dispose();
                m_data = null;
            }
        }

        public bool IsValid
        {
            get
            {
                return m_data != null && !m_data.IsNull;
            }
        }

        public T? Value
        {
            get
            {
                if (m_data == null)
                {
                    return default(T);
                }

                return (T?)m_data.GetValue();
            }
        }
    }
}