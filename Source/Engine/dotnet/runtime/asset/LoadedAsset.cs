using System.Runtime.InteropServices;

namespace Hyperion
{
    internal struct LoadedAssetNativeFunctions
    {
        [DllImport("hyperion", EntryPoint = "LoadedAsset_Destroy")]
        internal static extern void LoadedAsset_Destroy([In] IntPtr pLoadedAsset);

        [DllImport("hyperion", EntryPoint = "LoadedAsset_GetBoxed")]
        internal static extern void LoadedAsset_GetBoxed([In] IntPtr pLoadedAsset, [Out] out BoxedValueInternal outBoxed);
    }

    public class LoadedAsset : IDisposable
    {
        private BoxedValue? _data = null;

        public LoadedAsset(IntPtr pLoadedAsset)
        {
            if (pLoadedAsset != IntPtr.Zero)
            {
                BoxedValueInternal dataBuffer;
                LoadedAssetNativeFunctions.LoadedAsset_GetBoxed(pLoadedAsset, out dataBuffer);

                if (dataBuffer.IsNull)
                {
                    dataBuffer.Dispose();
                    return;
                }

                _data = BoxedValue.FromBuffer(dataBuffer);
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
        private BoxedValue? _data = null;

        public LoadedAsset(IntPtr pLoadedAsset)
        {
            if (pLoadedAsset != IntPtr.Zero)
            {
                BoxedValueInternal dataBuffer;
                LoadedAssetNativeFunctions.LoadedAsset_GetBoxed(pLoadedAsset, out dataBuffer);

                if (dataBuffer.IsNull)
                {
                    dataBuffer.Dispose();
                    return;
                }

                _data = BoxedValue.FromBuffer(dataBuffer);
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