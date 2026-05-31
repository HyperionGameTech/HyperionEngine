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

    internal delegate void HandleAssetResultDelegate(IntPtr assetPtr);

    [ClassBinding(Name = "AssetCollector")]
    public class AssetCollector : ObjectBase
    {
        private static readonly LogChannel _logChannel = LogChannel.ByName("Assets");

        private FileSystemWatcher? _watcher;
        public event Action<string, AssetChangeType> AssetChanged;

        public AssetCollector()
        {
        }

        public void StartWatching()
        {
            Logger.Log(_logChannel, LogLevel.Verbose, "Start watching: {0}", this.GetBasePath());

            _watcher = new FileSystemWatcher();
            _watcher.Path = this.GetBasePath();
            _watcher.IncludeSubdirectories = true;
            _watcher.NotifyFilter = NotifyFilters.LastWrite;
            _watcher.Filter = "*.*";

            _watcher.Changed += (source, e) =>
            {
                if (!IsHiddenOrSystemFile(e.FullPath))
                {
                    OnFileChanged(source, e);
                }
            };

            _watcher.Created += (source, e) =>
            {
                if (!IsHiddenOrSystemFile(e.FullPath))
                {
                    OnFileCreated(source, e);
                }
            };

            _watcher.Deleted += (source, e) =>
            {
                if (!IsHiddenOrSystemFile(e.FullPath))
                {
                    OnFileDeleted(source, e);
                }
            };

            _watcher.Renamed += (source, e) =>
            {
                if (!IsHiddenOrSystemFile(e.FullPath))
                {
                    OnFileRenamed(source, e as RenamedEventArgs);
                }
            };

            _watcher.EnableRaisingEvents = true;
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
            if (_watcher == null)
                return;

            _watcher.EnableRaisingEvents = false;
            _watcher.Dispose();
            _watcher = null;
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

    [ClassBinding(Name = "AssetManager")]
    public class AssetManager : ObjectBase
    {
        private static AssetManager? _instance = null;

        public static AssetManager Instance
        {
            get
            {
                if (_instance == null)
                {
                    using (BoxedValueInternal resultData = ObjectBase.GetMethod(Class.GetClass(typeof(AssetManager)), new Name("GetInstance", weak: true)).InvokeNative())
                    {
                        _instance = (AssetManager?)resultData.GetValue();

                        if (_instance == null)
                        {
                            throw new Exception("Failed to get AssetManager instance");
                        }
                    }
                }

                return _instance;
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

        public LoadedAsset<T> Load<T>(string path)
        {
            Class? cls = Class.TryGetClass<T>();

            if (cls == null)
                throw new Exception("Failed to get Class for type: " + typeof(T).Name + ", cannot load asset!");

            IntPtr loaderDefinitionPtr = AssetManager_GetLoaderDefinition(NativeAddress, path, ((Class)cls).TypeId);

            if (loaderDefinitionPtr == IntPtr.Zero)
                throw new Exception("Failed to get loader definition for path: " + path + ", cannot load asset!");

            return new LoadedAsset<T>(AssetManager_Load(NativeAddress, loaderDefinitionPtr, path));
        }

        public async Task<LoadedAsset<T>> LoadAsync<T>(string path)
        {
            Class? cls = Class.TryGetClass<T>();

            if (cls == null)
                throw new Exception("Failed to get Class for type: " + typeof(T).Name + ", cannot load asset!");

            IntPtr loaderDefinitionPtr = AssetManager_GetLoaderDefinition(NativeAddress, path, ((Class)cls).TypeId);

            if (loaderDefinitionPtr == IntPtr.Zero)
                throw new Exception("Failed to get loader definition for path: " + path + ", cannot load asset!");

            var completionSource = new TaskCompletionSource<LoadedAsset<T>>();

            AssetManager_LoadAsync(NativeAddress, Marshal.GetFunctionPointerForDelegate(new HandleAssetResultDelegate((assetPtr) =>
            {
                if (assetPtr == IntPtr.Zero)
                {
                    completionSource.SetException(new Exception("Failed to load asset"));

                    return;
                }

                completionSource.SetResult(new LoadedAsset<T>(assetPtr));
            })));

            return await completionSource.Task;
        }

        public AssetRegistry AssetRegistry => this.GetAssetRegistry();

        [DllImport("hyperion", EntryPoint = "AssetManager_GetLoaderDefinition")]
        private static extern IntPtr AssetManager_GetLoaderDefinition([In] IntPtr assetManagerPtr, [MarshalAs(UnmanagedType.LPStr)] string path, TypeId desiredTypeId);

        [DllImport("hyperion", EntryPoint = "AssetManager_LoadAsync")]
        private static extern void AssetManager_LoadAsync(IntPtr assetManagerPtr, IntPtr handleAssetResultPtr);

        [DllImport("hyperion", EntryPoint = "AssetManager_Load")]
        private static extern IntPtr AssetManager_Load([In] IntPtr assetManagerPtr, [In] IntPtr loaderDefinitionPtr, [MarshalAs(UnmanagedType.LPStr)] string path);

    }
}