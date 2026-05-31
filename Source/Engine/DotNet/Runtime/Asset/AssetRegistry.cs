using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetRegistry")]
    public class AssetRegistry : ObjectBase
    {
        public AssetRegistry()
        {
        }

        public IEnumerable<AssetDesc> GetBucketAssetDescs(uint bucketIndex)
        {
            uint count = AssetRegistry_GetBucketAssetDescs(NativeAddress, bucketIndex, IntPtr.Zero, 0);

            if (count == 0)
            {
                yield break;
            }

            Debug.Assert(Marshal.SizeOf<AssetDesc>() == 12);
            IntPtr buffer = Marshal.AllocHGlobal(Marshal.SizeOf<AssetDesc>() * (int)count);

            try
            {
                AssetRegistry_GetBucketAssetDescs(NativeAddress, bucketIndex, buffer, count);

                for (uint i = 0; i < count; i++)
                {
                    IntPtr currentPtr = IntPtr.Add(buffer, (int)(i * Marshal.SizeOf<AssetDesc>()));
                    AssetDesc assetDesc = Marshal.PtrToStructure<AssetDesc>(currentPtr);
                    yield return assetDesc;
                }
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

        public AssetObject? GetAsset(uint bucketIndex, Name name)
        {
            BoxedValueInternal dataBuffer;

            if (!AssetRegistry_GetAssetBoxed(NativeAddress, bucketIndex, ref name, out dataBuffer))
            {
                return null;
            }

            try
            {
                return dataBuffer.ReadObject<AssetObject>();
            }
            finally
            {
                dataBuffer.Dispose();
            }
        }

        [DllImport("hyperion", EntryPoint = "AssetRegistry_GetBucketAssetDescs")]
        private static extern uint AssetRegistry_GetBucketAssetDescs(IntPtr pRegistry, uint bucketIndex, IntPtr pOutAssetDescs, uint maxCount);

        [DllImport("hyperion", EntryPoint = "AssetRegistry_GetAssetBoxed")]
        private static extern bool AssetRegistry_GetAssetBoxed([In] IntPtr pRegistry, uint bucketIndex, [In] ref Name name, [Out] out BoxedValueInternal outBoxed);
    }
}
