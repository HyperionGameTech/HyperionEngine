using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class AssetPickerItemViewModel : ViewModelBase
    {
        public Name AssetName { get; }
        public uint BucketIndex { get; }
        public string DisplayName { get; }
        public string TypeName { get; }

        public AssetPickerItemViewModel(Name assetName, uint bucketIndex, string displayName, string typeName)
        {
            AssetName = assetName;
            BucketIndex = bucketIndex;
            DisplayName = displayName;
            TypeName = typeName;
        }

        /// used for filtering
        public override string ToString() => DisplayName;
    }
}
