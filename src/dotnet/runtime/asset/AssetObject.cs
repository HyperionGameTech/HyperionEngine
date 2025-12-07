using System;
using System.IO;
using System.Runtime.InteropServices;
using Hyperion;

namespace Hyperion
{
    [ClassBinding(Name = "AssetObjectFlags")]
    [Flags]
    public enum AssetObjectFlags : uint
    {
        None = 0x0,
        Persistent = 0x1
    }

    [ClassBinding(Name = "AssetObject")]
    public class AssetObject : ObjectBase
    {
        public AssetObject()
        {
        }

        public Name Name
        {
            get => this.GetName();
            set
            {
                var res = this.Rename(value);

                if (!res.IsValid)
                {
                    throw new InvalidOperationException($"Failed to rename AssetObject to '{value}': {res}");
                }
            }
        }
    }
}