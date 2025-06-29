using System;
using System.IO;
using System.Runtime.InteropServices;
using Hyperion;

namespace Hyperion
{
    public enum AssetChangeType : uint
    {
        Changed = 0,
        Created = 1,
        Deleted = 2,
        Renamed = 3
    }

    internal delegate void HandleAssetResultDelegate(IntPtr assetPtr);

    [HypClassBinding(Name="AssetCollector")]
    public class AssetCollector : HypObject
    {
        private static readonly LogChannel logChannel = LogChannel.ByName("Assets");

        private FileSystemWatcher watcher;
        public event Action<string, AssetChangeType> AssetChanged;

        public AssetCollector()
        {
        }

        public void StartWatching()
        {
            Logger.Log(logChannel, LogType.Debug, "Start watching: {0}", this.GetBasePath());

            watcher = new FileSystemWatcher();
            watcher.Path = this.GetBasePath();
            watcher.IncludeSubdirectories = true;
            watcher.NotifyFilter = NotifyFilters.LastWrite;
            watcher.Filter = "*.*";

            watcher.Changed += (source, e) =>
            {
                if (!IsHiddenOrSystemFile(e.FullPath))
                {
                    OnFileChanged(source, e);
                }
            };

            watcher.Created += (source, e) =>
            {
                if (!IsHiddenOrSystemFile(e.FullPath))
                {
                    OnFileCreated(source, e);
                }
            };

            watcher.Deleted += (source, e) =>
            {
                if (!IsHiddenOrSystemFile(e.FullPath))
                {
                    OnFileDeleted(source, e);
                }
            };

            watcher.Renamed += (source, e) =>
            {
                if (!IsHiddenOrSystemFile(e.FullPath))
                {
                    OnFileRenamed(source, e as RenamedEventArgs);
                }
            };

            watcher.EnableRaisingEvents = true;
        }

        private bool IsHiddenOrSystemFile(string path)
        {
            try
            {
                FileAttributes attributes = File.GetAttributes(path);

                return (attributes & FileAttributes.Hidden) == FileAttributes.Hidden || 
                    (attributes & FileAttributes.System) == FileAttributes.System;
            }
            catch (FileNotFoundException)
            {
                // If the file doesn't exist, we can't determine its attributes
                return false;
            }
            catch (UnauthorizedAccessException)
            {
                // If we don't have permission to access the file, assume it's not hidden or system
                return false;
            }
            catch (Exception ex)
            {
                throw;
            }
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
            this.NotifyAssetChanged(e.FullPath, AssetChangeType.Changed);
        }

        private void OnFileCreated(object source, FileSystemEventArgs e)
        {
            this.NotifyAssetChanged(e.FullPath, AssetChangeType.Created);
        }

        private void OnFileDeleted(object source, FileSystemEventArgs e)
        {
            this.NotifyAssetChanged(e.FullPath, AssetChangeType.Deleted);
        }

        private void OnFileRenamed(object source, RenamedEventArgs e)
        {
            this.NotifyAssetChanged(e.OldFullPath, AssetChangeType.Renamed);
        }
    }
    
    [HypClassBinding(Name="AssetManager")]
    public class AssetManager : HypObject
    {
        private static AssetManager? instance = null;
        
        public static AssetManager Instance
        {
            get
            {
                if (instance == null)
                {
                    using (HypDataBuffer resultData = HypObject.GetMethod(HypClass.GetClass(typeof(AssetManager)), new Name("GetInstance", weak: true)).InvokeNative())
                    {
                        instance = (AssetManager)resultData.GetValue();
                    }
                }

                return (AssetManager)instance;
            }
        }

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

        public Asset<T> Load<T>(string path)
        {
            HypClass? hypClass = HypClass.TryGetClass<T>();

            if (hypClass == null)
                throw new Exception("Failed to get HypClass for type: " + typeof(T).Name + ", cannot load asset!");

            IntPtr loaderDefinitionPtr = AssetManager_GetLoaderDefinition(NativeAddress, path, ((HypClass)hypClass).TypeId);

            if (loaderDefinitionPtr == IntPtr.Zero)
                throw new Exception("Failed to get loader definition for path: " + path + ", cannot load asset!");

            return new Asset<T>(AssetManager_Load(NativeAddress, loaderDefinitionPtr, path));
        }

        public async Task<Asset<T>> LoadAsync<T>(string path)
        {
            HypClass? hypClass = HypClass.TryGetClass<T>();

            if (hypClass == null)
                throw new Exception("Failed to get HypClass for type: " + typeof(T).Name + ", cannot load asset!");

            IntPtr loaderDefinitionPtr = AssetManager_GetLoaderDefinition(NativeAddress, path, ((HypClass)hypClass).TypeId);

            if (loaderDefinitionPtr == IntPtr.Zero)
                throw new Exception("Failed to get loader definition for path: " + path + ", cannot load asset!");

            var completionSource = new TaskCompletionSource<Asset<T>>();
            
            AssetManager_LoadAsync(NativeAddress, Marshal.GetFunctionPointerForDelegate(new HandleAssetResultDelegate((assetPtr) =>
            {
                if (assetPtr == IntPtr.Zero)
                {
                    completionSource.SetException(new Exception("Failed to load asset"));

                    return;
                }

                completionSource.SetResult(new Asset<T>(assetPtr));
            })));

            return await completionSource.Task;
        }

        [DllImport("hyperion", EntryPoint = "AssetManager_GetLoaderDefinition")]
        private static extern IntPtr AssetManager_GetLoaderDefinition([In] IntPtr assetManagerPtr, [MarshalAs(UnmanagedType.LPStr)] string path, TypeId desiredTypeId);

        [DllImport("hyperion", EntryPoint = "AssetManager_LoadAsync")]
        private static extern void AssetManager_LoadAsync(IntPtr assetManagerPtr, IntPtr handleAssetResultPtr);

        [DllImport("hyperion", EntryPoint = "AssetManager_Load")]
        private static extern IntPtr AssetManager_Load([In] IntPtr assetManagerPtr, [In] IntPtr loaderDefinitionPtr, [MarshalAs(UnmanagedType.LPStr)] string path);

    }
}