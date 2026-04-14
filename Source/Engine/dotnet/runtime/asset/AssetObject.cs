using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetObjectFlags")]
    [Flags]
    public enum AssetObjectFlags : uint
    {
        None = 0x0,
        Persistent = 0x1
    }

    [ClassBinding(Name = "AssetDesc")]
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct AssetDesc
    {
        public static readonly uint InvalidIndex = 0;

        public Name Name;
        public uint Index;
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

        public Name FriendlyName
        {
            get => this.GetFriendlyName();      // extension method
            set => this.SetFriendlyName(value); // extension method
        }

        public AssetPath Path => this.IsRegistered() ? this.GetPath() : AssetPath.Invalid; // extension method
    }
}