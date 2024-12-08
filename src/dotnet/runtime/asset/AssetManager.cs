using System;
using System.IO;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum AssetChangeType : uint
    {
        Changed = 0,
        Created = 1,
        Deleted = 2,
        Renamed = 3
    }

    [HypClassBinding(Name="AssetCollector")]
    public class AssetCollector : HypObject
    {
        private FileSystemWatcher watcher;
        public event Action<string, AssetChangeType> AssetChanged;

        public AssetCollector()
        {
        }

        public void StartWatching()
        {
            Logger.Log(LogType.Info, "Start watching: {0}", this.GetBasePath());

            watcher = new FileSystemWatcher();
            watcher.Path = this.GetBasePath();
            watcher.IncludeSubdirectories = true;
            watcher.NotifyFilter = NotifyFilters.LastWrite;
            watcher.Filter = "*.*";
            watcher.Changed += OnFileChanged;
            watcher.Created += OnFileCreated;
            watcher.Deleted += OnFileDeleted;
            watcher.Renamed += OnFileRenamed;
            watcher.EnableRaisingEvents = true;
        }

        public void StopWatching()
        {
            watcher.EnableRaisingEvents = false;
            watcher.Dispose();

            watcher = null;
        }

        public void OnAssetChanged(string path, AssetChangeType changeType)
        {
            AssetChanged?.Invoke(path, changeType);
        }

        private void OnFileChanged(object source, FileSystemEventArgs e)
        {
            Logger.Log(LogType.Info, "File changed: {0} {1}", e.FullPath, e.ChangeType);

            this.NotifyAssetChanged(e.FullPath, AssetChangeType.Changed);
        }

        private void OnFileCreated(object source, FileSystemEventArgs e)
        {
            Logger.Log(LogType.Info, "File created: {0} {1}", e.FullPath, e.ChangeType);

            this.NotifyAssetChanged(e.FullPath, AssetChangeType.Created);
        }

        private void OnFileDeleted(object source, FileSystemEventArgs e)
        {
            Logger.Log(LogType.Info, "File deleted: {0} {1}", e.FullPath, e.ChangeType);

            this.NotifyAssetChanged(e.FullPath, AssetChangeType.Deleted);
        }

        private void OnFileRenamed(object source, RenamedEventArgs e)
        {
            Logger.Log(LogType.Info, "File renamed: {0} {1}", e.OldFullPath, e.FullPath);

            this.NotifyAssetChanged(e.OldFullPath, AssetChangeType.Renamed);
        }
    }
    
    [HypClassBinding(Name="AssetManager")]
    public class AssetManager : HypObject
    {
        public AssetManager()
        {
        }

        public string BasePath
        {
            get
            {
                return (string)GetProperty(PropertyNames.BasePath)
                    .Get(this)
                    .GetValue();
            }
        }
    }
}