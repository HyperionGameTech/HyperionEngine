namespace Hyperion.Editor.ViewModels
{
    public enum ContentItemType
    {
        Directory,
        Asset
    }

    public class ContentItemViewModel : ViewModelBase
    {
        public ContentItemViewModel(string name, string fullPath, ContentItemType itemType, ContentDirectoryViewModel? directory = null)
        {
            Name = name;
            FullPath = fullPath;
            ItemType = itemType;
            Directory = directory;
        }

        public string Name { get; }
        public string FullPath { get; }
        public ContentItemType ItemType { get; }
        public ContentDirectoryViewModel? Directory { get; }

        public string DisplayName => Name;

        public static ContentItemViewModel FromDirectory(ContentDirectoryViewModel directory)
        {
            return new ContentItemViewModel(directory.Name, directory.FullPath, ContentItemType.Directory, directory);
        }
    }
}
