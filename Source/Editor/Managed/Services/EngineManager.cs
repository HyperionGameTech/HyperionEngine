using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.IO;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Reflection;
using Hyperion;

namespace Hyperion.Editor
{
    public static class EngineManager
    {

        public static bool IsInitialized { get; private set; }
        public static bool DisableMainLoop { get; set; } = false;

        public static EditorGame? EditorGame { get; private set; }
        public static Game? GameInstance { get; private set; } // Local copy of EngineDriver.Instance.GameInstance

        public static EditorProject? CurrentProject { get; private set; }

        public static string? ProjectLoadStatus { get; private set; }

        // raised on the sim thread
        public static event Action<string?>? ProjectLoadStatusChanged;

        private static DelegateHandler? _onProjectLoadStatusChanged;
        private static DelegateHandler? _onProjectLoadFinished;

        private static List<EditorViewport> _registeredViewports = new List<EditorViewport>();
        private static Lock _lockViewports = new();

        private static DelegateHandler? _onCurrentProjectChanged;

        private static DelegateHandler? _gameLaunchedHandler;

        private static DelegateHandler? _onSceneAddedHandler;
        private static DelegateHandler? _onSceneRemovedHandler;

        private static DelegateHandler? _onTaskStartedHandler;
        private static DelegateHandler? _onTaskEndedHandler;
        private static DelegateHandler? _onTaskProgressUpdatedHandler;
        private static DelegateHandler? _onBuildNativeGameRequestedHandler;
        private static DelegateHandler? _onBuildWebGameRequestedHandler;
        private static DelegateHandler? _onRunWebGameRequestedHandler;
        private static DelegateHandler? _onBuildPackageRequestedHandler;
        private static DelegateHandler? _onRebuildNativeGameRequestedHandler;
        private static DelegateHandler? _onViewportLayoutRequestedHandler;

        public static event Action<World, Scene>? SceneAdded;
        public static event Action<World, Scene>? SceneRemoved;

        public static event Action<EditorTaskBase, bool>? TaskStarted;
        public static event Action<ObjIdBase>? TaskEnded;
        public static event Action<ObjIdBase, float>? TaskProgressUpdated;

        public static event Action? EditorInitialized;

        // raised on the sim thread
        public static event Action<uint>? ViewportLayoutRequested;

        private static bool _editorViewportsEnabled = false;

        // a plugin's assemblies (Hyperion.Plugin.<Module>) load when the engine loads its modules (OnPluginModuleLoaded)
        private static ReadOnlySpan<string> CoreAssemblyNames => new[] {
            EngineAssemblies.FileName(EngineAssemblies.Shared),
            EngineAssemblies.FileName(EngineAssemblies.Runtime),
            "Hyperion.Editor.dll"
        };

        public static void Initialize()
        {
            if (IsInitialized)
                return;

            unsafe
            {
                // Needed before calling Hyp_Initialize
                NativeBindings.Hyp_SetInitFromManagedCallback(&InitFromManagedCallback);
            }

            // create argv for NativeBindings.Hyp_Initialize
            List<string> args = [
                Environment.ProcessPath ?? "",
                "--detached",
                "--editor",

                // keep both sim and render threads independent because main thread will be locked at 30hz
                "-SimulateOnMainThread=false",
                "-RenderOnMainThread=false"
            ];

            args.AddRange(Environment.GetCommandLineArgs().Skip(1));

            int argc = args.Count;
            IntPtr argv = IntPtr.Zero;
            IntPtr[] argsPtrs = new IntPtr[argc];

            // Initialize Hyperion Engine
            try
            {
                for (int i = 0; i < argc; i++)
                {
                    argsPtrs[i] = Marshal.StringToCoTaskMemUTF8(args[i]);
                }

                argv = Marshal.AllocHGlobal(IntPtr.Size * argc);

                for (int i = 0; i < argc; i++)
                {
                    Marshal.WriteIntPtr(argv, i * IntPtr.Size, argsPtrs[i]);
                }

                if (NativeBindings.Hyp_Initialize(argc, argv) == 0)
                {
                    throw new Exception("Failed to initialize Hyperion Engine. NativeBindings.Hyp_Initialize returned false.");
                }

                BindProjectLoadStatus();

                NativeBindings.Hyp_LaunchThreads();
            }
            catch (Exception ex)
            {
                throw new Exception("Exception during engine startup: " + ex.Message, ex);
            }
            finally
            {
                for (int i = 0; i < argc; i++)
                {
                    if (argsPtrs[i] != IntPtr.Zero)
                    {
                        Marshal.FreeCoTaskMem(argsPtrs[i]);
                    }
                }

                if (argv != IntPtr.Zero)
                    Marshal.FreeHGlobal(argv);
            }

            InitializeEditorTasks();

            IsInitialized = true;
        }

        private static void BindProjectLoadStatus()
        {
            EditorState editorState = EditorState.Instance;
            Debug.Assert(editorState != null, "Failed to get EditorState instance");

            _onProjectLoadStatusChanged?.Remove();
            _onProjectLoadStatusChanged = editorState.GetOnProjectLoadStatusChangedDelegate().Bind((string status, uint percent) =>
            {
                SetProjectLoadStatus($"{status}... {percent}%");
            });

            _onProjectLoadFinished?.Remove();
            _onProjectLoadFinished = editorState.GetOnCurrentProjectChangedDelegate().Bind((EditorProject? newProject, bool isSimulationStateChange) =>
            {
                if (newProject != null)
                {
                    SetProjectLoadStatus(null);
                }
            });
        }

        private static void SetProjectLoadStatus(string? status)
        {
            ProjectLoadStatus = status;
            ProjectLoadStatusChanged?.Invoke(status);
        }

        private static void InitializeEditorTasks()
        {
            EditorState editorState = EditorState.Instance;
            Debug.Assert(editorState != null, "Failed to get EditorState instance");

            _onTaskStartedHandler?.Remove();
            _onTaskStartedHandler = editorState.GetOnTaskStartedDelegate().Bind((EditorTaskBase task) =>
            {
                RaiseTaskStarted(task, task.IsForegroundTask);
            });

            _onTaskEndedHandler?.Remove();
            _onTaskEndedHandler = editorState.GetOnTaskEndedDelegate().Bind((EditorTaskBase task) =>
            {
                RaiseTaskEnded(task.Id);
            });

            _onTaskProgressUpdatedHandler?.Remove();
            _onTaskProgressUpdatedHandler = editorState.GetOnTaskProgressUpdatedDelegate().Bind((EditorTaskBase task) =>
            {
                RaiseTaskProgressUpdated(task.Id, task.Progress);
            });

            _onBuildNativeGameRequestedHandler?.Remove();
            _onBuildNativeGameRequestedHandler = editorState.GetOnBuildNativeGameRequestedDelegate().Bind(() =>
            {
                string? projectFilePath = CurrentProject?.FilePath;

                if (Services.ManagedGameBuildService.FindProjectFile(projectFilePath) != null)
                {
                    _ = Task.Run(() => Services.ManagedGameBuildService.Instance.BuildAsync(projectFilePath));

                    return;
                }

                _ = Task.Run(() => Services.NativeGameBuildService.Instance.BuildAsync(projectFilePath));
            });

            _onBuildWebGameRequestedHandler?.Remove();
            _onBuildWebGameRequestedHandler = editorState.GetOnBuildWebGameRequestedDelegate().Bind((bool run) =>
            {
                string? projectFilePath = CurrentProject?.FilePath;

                _ = Task.Run(() => Services.WebGameBuildService.Instance.BuildAsync(projectFilePath, run));
            });

            _onRunWebGameRequestedHandler?.Remove();
            _onRunWebGameRequestedHandler = editorState.GetOnRunWebGameRequestedDelegate().Bind((bool hasUnsavedChanges) =>
            {
                string? projectFilePath = CurrentProject?.FilePath;

                _ = Task.Run(() => Services.WebGameBuildService.Instance.RunInBrowserAsync(projectFilePath, hasUnsavedChanges));
            });

            _onBuildPackageRequestedHandler?.Remove();
            _onBuildPackageRequestedHandler = editorState.GetOnBuildPackageRequestedDelegate().Bind((string projectFilePath, string binariesDirectory, string outputDirectory) =>
            {
                return Services.ManagedGameBuildService.Instance.BuildPackage(projectFilePath, binariesDirectory, outputDirectory);
            });

            // called from the project load, which waits for the build
            _onRebuildNativeGameRequestedHandler?.Remove();
            _onRebuildNativeGameRequestedHandler = editorState.GetOnRebuildNativeGameRequestedDelegate().Bind((string projectFilePath) =>
            {
                return Services.NativeGameBuildService.Instance.BuildForProjectLoad(projectFilePath);
            });

            _onViewportLayoutRequestedHandler?.Remove();
            _onViewportLayoutRequestedHandler = editorState.GetOnViewportLayoutRequestedDelegate().Bind((uint layout) =>
            {
                ViewportLayoutRequested?.Invoke(layout);
            });
        }

        public static void InitializeEditor()
        {
            EditorGame ??= new EditorGame();
            GameInstance = EditorGame;

            EditorState editorState = EditorState.Instance;
            Debug.Assert(editorState != null, "Failed to get EditorState instance");

            EngineDriver.Instance.GameInstance = EditorGame;

            CurrentProject = editorState.CurrentProject;

            BindWorldSceneDelegates(CurrentProject?.World);

            SyncEditorPlugins();

            _onCurrentProjectChanged?.Remove();
            _onCurrentProjectChanged = editorState.GetOnCurrentProjectChangedDelegate().Bind((EditorProject newProject, bool isSimulationStateChange) =>
            {
                Debug.Assert(newProject != CurrentProject);

                // If we are just switching to simulate mode, we don't want to dispose the project
                // Otherwise, when returning to edit mode, the project's managed resources would be cleaned up and cause a crash.
                bool shouldDisposeCurrentProject = CurrentProject != null && !isSimulationStateChange;

                if (shouldDisposeCurrentProject)
                {
                    // Dispose it early to ensure clean up is done in a timely manner, rather than wait for finalization.
                    CurrentProject?.Dispose();
                }
                else
                {
                    // @FIXME We have ourself a bug here, when opening a proj i'm seeing this logged.
                    Logger.Log(LogLevel.Debug, "NOT disposing CurrentProject (switching to simulate): " + (CurrentProject != null ? CurrentProject.Name : "null"));
                }

                CurrentProject = newProject;

                Logger.Log(LogLevel.Verbose, "Current project changed to: " + (CurrentProject != null ? CurrentProject.Name : "null"));

                SyncEditorPlugins();

                // When returning to edit mode from simulation, isSimulationStateChange is true and the scene delegates
                // are left unbound here - InitializeEditor() is called afterwards and rebinds them for the edit world.
                // The !isSimulationStateChange check is there because World may be null if it is just simulation state
                // change and world is loading
                if (CurrentProject != null && !isSimulationStateChange)
                {
                    World? world = CurrentProject.World;
                    Debug.Assert(world != null);

                    BindWorldSceneDelegates(world);
                }
                else
                {
                    BindWorldSceneDelegates(null);
                }
            });

            SetEditorViewportsEnabled(true);

            EditorInitialized?.Invoke();
        }

        // Also rebound by MainWindowViewModel when the edited World changes
        internal static void BindWorldSceneDelegates(World? world)
        {
            _onSceneAddedHandler?.Remove();
            _onSceneRemovedHandler?.Remove();

            if (world == null)
            {
                return;
            }

            _onSceneAddedHandler = world.GetOnSceneAddedDelegate().Bind((World world, Scene scene) =>
            {
                SceneAdded?.Invoke(world, scene);
            });

            _onSceneRemovedHandler = world.GetOnSceneRemovedDelegate().Bind((World world, Scene scene) =>
            {
                SceneRemoved?.Invoke(world, scene);
            });
        }

        public static void InitializeGame(Game game)
        {
            if (game is EditorGame)
            {
                throw new ArgumentException("InitializeGame() shouldn't be called with an instance of EditorGame - use InitializeEditor() instead");
            }

            World? world = game.World;
            Debug.Assert(world != null);

            BindWorldSceneDelegates(world);

            EngineDriver.Instance.GameInstance = game;
            GameInstance = game;

            SetEditorViewportsEnabled(false);
        }

        public static void Shutdown()
        {
            _gameLaunchedHandler?.Remove();
            _onCurrentProjectChanged?.Remove();
            _onProjectLoadStatusChanged?.Remove();
            _onProjectLoadFinished?.Remove();
            _onSceneAddedHandler?.Remove();
            _onSceneRemovedHandler?.Remove();

            GameInstance = null;
            EditorGame = null;

            ObjectBase.IsEngineShuttingDown = true;

            Services.PlayInEditorServerService.Instance.Dispose();

            NativeBindings.Hyp_Shutdown();

            IsInitialized = false;
        }

        internal static void RaiseTaskStarted(EditorTaskBase task, bool isForegroundTask)
        {
            TaskStarted?.Invoke(task, isForegroundTask);
        }

        internal static void RaiseTaskEnded(ObjIdBase taskId)
        {
            TaskEnded?.Invoke(taskId);
        }

        internal static void RaiseTaskProgressUpdated(ObjIdBase taskId, float progress)
        {
            TaskProgressUpdated?.Invoke(taskId, progress);
        }

        [UnmanagedCallersOnly]
        private static unsafe void InitFromManagedCallback(ManagedDelegates* pManagedDelegates)
        {
            Debug.Assert(pManagedDelegates != null, "pManagedDelegates is null in InitFromManagedCallback");

            // Initialize Hyperion.NET runtime
            int res = NativeInterop.InitializeRuntimeManaged();
            if (res != (int)LoadAssemblyResult.Ok)
            {
                throw new Exception("Failed to initialize Hyperion .NET runtime from managed code. Error code: " + (LoadAssemblyResult)res);
            }

            pManagedDelegates->initializeAssembly = (delegate* unmanaged<IntPtr, IntPtr, IntPtr, int, int>)&NativeInterop.InitializeAssembly;
            pManagedDelegates->unloadAssembly = (delegate* unmanaged<IntPtr, IntPtr, void>)&NativeInterop.UnloadAssembly;
            pManagedDelegates->pluginModuleLoaded = &OnPluginModuleLoaded;

            IntPtr assemblyGuidPtr = Marshal.AllocHGlobal(Marshal.SizeOf<Guid>());
            IntPtr assemblyPathPtr = IntPtr.Zero;

            try
            {
                foreach (string assemblyName in CoreAssemblyNames)
                {
                    string assemblyPath = Path.Combine(AppContext.BaseDirectory, assemblyName);
                    assemblyPathPtr = Marshal.StringToCoTaskMemUTF8(assemblyPath);

                    res = NativeInterop.InitializeAssemblyManaged(assemblyGuidPtr, IntPtr.Zero, assemblyPathPtr, /* isCoreAssembly */ 1, out Exception? error);

                    Marshal.FreeCoTaskMem(assemblyPathPtr);
                    assemblyPathPtr = IntPtr.Zero;

                    if (res != (int)LoadAssemblyResult.Ok)
                    {
                        throw new Exception("Failed to initialize assembly at: " + assemblyPath + ". Error code: " + (LoadAssemblyResult)res
                            + (error != null ? "\n" + error.GetType().Name + ": " + error.Message : string.Empty), error);
                    }
                }
            }
            catch (Exception ex)
            {
                throw new Exception("Exception during Hyperion .NET runtime initialization: " + ex.Message, ex);
            }
            finally
            {
                if (assemblyPathPtr != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(assemblyPathPtr);
                }

                if (assemblyGuidPtr != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(assemblyGuidPtr);
                }
            }
        }

        // a project enabled a built-in plugin: its module's assembly, if it has one, needs the module's classes, which exist from now on
        [UnmanagedCallersOnly]
        private static unsafe void OnPluginModuleLoaded(IntPtr moduleNamePtr)
        {
            string assemblyName = EngineAssemblies.PluginPrefix + (Marshal.PtrToStringUTF8(moduleNamePtr) ?? string.Empty);
            string assemblyPath = Path.Combine(AppContext.BaseDirectory, EngineAssemblies.FileName(assemblyName));

            if (!File.Exists(assemblyPath))
            {
                return;
            }

            IntPtr assemblyGuidPtr = Marshal.AllocHGlobal(Marshal.SizeOf<Guid>());
            IntPtr assemblyPathPtr = Marshal.StringToCoTaskMemUTF8(assemblyPath);

            try
            {
                int res = NativeInterop.InitializeAssemblyManaged(assemblyGuidPtr, IntPtr.Zero, assemblyPathPtr, /* isCoreAssembly */ 1, out Exception? error);

                if (res != (int)LoadAssemblyResult.Ok)
                {
                    Logger.Log(LogLevel.Error, "Failed to initialize plugin assembly {0}: {1}{2}", assemblyName, (LoadAssemblyResult)res, error != null ? " (" + error.Message + ")" : string.Empty);

                    return;
                }

                if (AssemblyCache.Instance.Get(assemblyPath)?.Assembly is Assembly assembly)
                {
                    Services.EditorPluginService.Instance.AddPluginAssembly(assembly);
                }
            }
            finally
            {
                Marshal.FreeCoTaskMem(assemblyPathPtr);
                Marshal.FreeHGlobal(assemblyGuidPtr);
            }
        }

        public static bool IsOnSimThread => SimThread.IsOnIt;

        // the native side enables a project's plugins before its World initializes; the editor UI follows when the project changes
        private static void SyncEditorPlugins()
        {
            List<string> enabledPlugins = new List<string>();

            if (CurrentProject != null)
            {
                foreach (Name pluginName in CurrentProject.GetEnabledPlugins().Cast<Name>())
                {
                    enabledPlugins.Add(pluginName.ToString());
                }
            }

            Avalonia.Threading.Dispatcher.UIThread.Post(() => Services.EditorPluginService.Instance.SetEnabledPlugins(enabledPlugins));
        }

        public static bool CanCreateAssets => EditorGame?.EditorSubsystem?.CanCreateAssets() ?? false;

        public static bool IsStrataEnabled => EditorGame?.EditorSubsystem?.IsStrataEnabled() ?? false;

        public static bool CanChangeGameProjectLanguage => EditorGame?.EditorSubsystem?.CanChangeGameProjectLanguage() ?? false;

        public static EditorActionStack? GetActionStackFor(ObjectBase? owner)
        {
            EditorSubsystem? editorSubsystem = EditorGame?.EditorSubsystem;

            if (editorSubsystem == null)
            {
                return CurrentProject?.ActionStack;
            }

            return owner is Node node
                ? editorSubsystem.GetActionStackForNode(node)
                : editorSubsystem.GetSceneActionStack();
        }

        public static async Task PostToSimThread(Action action)
        {
            await SimThread.PostTask(action).ConfigureAwait(false);
        }

        public static Task WriteGameArgs(string? projectFilePath)
        {
            return PostToSimThread(() =>
            {
                EditorProject? project = CurrentProject;

                if (string.IsNullOrEmpty(projectFilePath) || project == null || !project.IsValid
                    || !string.Equals(Path.GetFullPath(project.FilePath), Path.GetFullPath(projectFilePath), StringComparison.OrdinalIgnoreCase))
                {
                    return;
                }

                if (!project.WriteGameArgs().IsValid)
                {
                    Logger.Log(LogLevel.Warning, "Failed to write the game's args.txt");
                }
            });
        }

        public static async Task<T> PostToSimThread<T>(Func<T> func)
        {
            return await SimThread.PostTask(func).ConfigureAwait(false);
        }

        public static void RegisterViewport(EditorViewport viewport)
        {
            _lockViewports.Enter();

            if (_registeredViewports.Contains(viewport))
            {
                _lockViewports.Exit();
                return;
            }

            _registeredViewports.Add(viewport);

            _lockViewports.Exit();

            if (EditorGame == null)
            {
                return;
            }

            if (EditorGame.IsLaunched())
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    if (!_editorViewportsEnabled)
                    {
                        return;
                    }

                    _lockViewports.Enter();

                    try
                    {
                        EditorSubsystem? editorSubsystem = EditorGame.EditorSubsystem;
                        if (editorSubsystem == null)
                        {
                            throw new InvalidOperationException("EditorSubsystem is not initialized!");
                        }

                        // check still registered
                        EditorViewport? registeredViewport = _registeredViewports.Find(v => v.Id == viewport.Id);
                        if (registeredViewport == null)
                        {
                            Logger.Log(LogLevel.Warning, $"EditorViewport {viewport.Id} is no longer registered - skipping addition to EditorSubsystem.");
                            return;
                        }

                        editorSubsystem.AddViewport(viewport);
                    }
                    finally
                    {
                        _lockViewports.Exit();
                    }
                });

                return;
            }

            if (_gameLaunchedHandler != null)
            {
                return;
            }

            // not launched; add every viewport registered by then once it is
            _gameLaunchedHandler = EditorGame.GetOnLaunchedDelegate().Bind(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    // if set to disabled in the time between
                    if (!_editorViewportsEnabled)
                    {
                        return;
                    }

                    _lockViewports.Enter();

                    try
                    {
                        EditorSubsystem? editorSubsystem = EditorGame.EditorSubsystem;
                        if (editorSubsystem == null)
                        {
                            throw new InvalidOperationException("EditorSubsystem is not initialized!");
                        }

                        foreach (EditorViewport registeredViewport in _registeredViewports)
                        {
                            editorSubsystem.AddViewport(registeredViewport);
                        }
                    }
                    finally
                    {
                        _lockViewports.Exit();
                    }
                });
            });
        }

        public static void UnregisterViewport(EditorViewport viewport, bool removeFromList = true)
        {
            _lockViewports.Enter();

            if (removeFromList)
            {
                if (!_registeredViewports.Remove(viewport))
                {
                    _lockViewports.Exit();
                    return;
                }
            }

            _lockViewports.Exit();

            if (EditorGame == null)
            {
                return;
            }

            if (EditorGame.IsLaunched())
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    _lockViewports.Enter();

                    try
                    {
                        EditorSubsystem? editorSubsystem = EditorGame.EditorSubsystem;
                        if (editorSubsystem == null)
                        {
                            throw new InvalidOperationException("EditorSubsystem is not initialized!");
                        }

                        // check it is still not contained in the list
                        if (_registeredViewports.Find(v => v.Id == viewport.Id) != null)
                        {
                            Logger.Log(LogLevel.Warning, $"EditorViewport {viewport.Id} is still registered (re-added?) - skipping removal from EditorSubsystem.");
                            return;
                        }

                        editorSubsystem.RemoveViewport(viewport);
                    }
                    finally
                    {
                        _lockViewports.Exit();
                    }
                });
            }
        }

        /// <summary>
        /// Disables/enables all registered editor viewports by removing or adding them to the EditorSubsystem. Calls on
        /// the sim thread.
        /// </summary>
        public static void SetEditorViewportsEnabled(bool enabled)
        {
            if (_editorViewportsEnabled == enabled)
            {
                // already in desired state
                return;
            }

            _editorViewportsEnabled = enabled;

            if (EditorGame == null || !EditorGame.IsLaunched())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                if (_editorViewportsEnabled != enabled)
                {
                    // state changed since here
                    return;
                }

                _lockViewports.Enter();

                try
                {
                    EditorSubsystem? editorSubsystem = EditorGame.EditorSubsystem;
                    if (editorSubsystem == null)
                    {
                        throw new InvalidOperationException("EditorSubsystem is not initialized!");
                    }

                    if (enabled)
                    {
                        foreach (EditorViewport vp in _registeredViewports)
                        {
                            editorSubsystem.AddViewport(vp);
                        }
                    }
                    else
                    {
                        foreach (EditorViewport vp in _registeredViewports)
                        {
                            editorSubsystem.RemoveViewport(vp);
                        }
                    }
                }
                finally
                {
                    _lockViewports.Exit();
                }
            });
        }
    }
}
