using System;
using System.IO;
using System.Runtime.InteropServices;
using Hyperion;

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
    }
}