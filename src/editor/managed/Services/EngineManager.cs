using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Runtime.InteropServices;
using System.Diagnostics;
using Hyperion;

namespace Hyperion.Editor
{
    public static class EngineManager
    {

        public static bool IsInitialized { get; private set; }
        public static HyperionEditorGame? EditorGame { get; private set; }
        public static Game? GameInstance { get; private set; } // Local copy of EngineDriver.Instance.GameInstance

        public static EditorProject? CurrentProject { get; private set; }


        private static List<EditorViewport> _registeredViewports = new List<EditorViewport>();
        private static Lock _lockViewports = new Lock();

        private static DelegateHandler? _onCurrentProjectChanged;

        private static DelegateHandler? _gameLaunchedHandler;

        private static DelegateHandler? _onSceneAddedHandler;
        private static DelegateHandler? _onSceneRemovedHandler;

        private static DelegateHandler? _onTaskStartedHandler;
        private static DelegateHandler? _onTaskEndedHandler;
        private static DelegateHandler? _onTaskProgressUpdatedHandler;

        public static event Action<World, Scene>? SceneAdded;
        public static event Action<World, Scene>? SceneRemoved;

        public static event Action<EditorTaskBase, bool>? TaskStarted;
        public static event Action<ObjIdBase>? TaskEnded;
        public static event Action<ObjIdBase, float>? TaskProgressUpdated;

        private static bool _editorViewportsEnabled = false;

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
                "-Headless",
                "-Detached",
                "-Editor",

                // keep both sim and render threads independent because main thread will be locked at 30hz
                "-SimulateOnMainThread=false",
                "-RenderOnMainThread=false"
            ];

            int argc = args.Count;
            IntPtr argv = IntPtr.Zero;
            IntPtr[] argsPtrs = new IntPtr[argc];

            // Initialize Hyperion Engine
            try
            {
                for (int i = 0; i < argc; i++)
                {
                    argsPtrs[i] = Marshal.StringToHGlobalAnsi(args[i]);
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

                InitializeEditorTasks();

                NativeBindings.Hyp_LaunchThreads();
            }
            catch (Exception ex)
            {
                throw new Exception("Exception during Hyperion Engine initialization!", ex);
            }
            finally
            {
                for (int i = 0; i < argc; i++)
                {
                    if (argsPtrs[i] != IntPtr.Zero)
                    {
                        Marshal.FreeHGlobal(argsPtrs[i]);
                    }
                }

                if (argv != IntPtr.Zero)
                    Marshal.FreeHGlobal(argv);
            }

            IsInitialized = true;
        }

        private static void InitializeEditorTasks()
        {
            EditorState editorState = EditorState.Instance;
            Debug.Assert(editorState != null, "Failed to get EditorState instance");

            _onTaskStartedHandler?.Remove();
            _onTaskStartedHandler = editorState.GetOnTaskStartedDelegate().Bind((EditorTaskBase task) =>
            {
                bool isForegroundTask = false;

                if (task is TickableEditorTask tickableTask)
                {
                    isForegroundTask = tickableTask.IsForegroundTask;
                }

                RaiseTaskStarted(task, isForegroundTask);
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
        }

        public static void InitializeEditor()
        {
            EditorGame ??= new HyperionEditorGame();
            GameInstance = EditorGame;

            EditorState editorState = EditorState.Instance;
            Debug.Assert(editorState != null, "Failed to get EditorState instance");

            EngineDriver.Instance.GameInstance = EditorGame;

            CurrentProject = editorState.CurrentProject;

            _onSceneAddedHandler?.Remove();
            _onSceneRemovedHandler?.Remove();

            _onCurrentProjectChanged?.Remove();
            _onCurrentProjectChanged = editorState.GetOnCurrentProjectChangedDelegate().Bind((EditorProject newProject) =>
            {
                CurrentProject = newProject;

                Logger.Log(LogType.Info, "Current project changed to: " + (CurrentProject != null ? CurrentProject.Name : "null"));

                if (CurrentProject != null)
                {
                    _onSceneAddedHandler = CurrentProject.World.GetOnSceneAddedDelegate().Bind((World world, Scene scene) =>
                    {
                        SceneAdded?.Invoke(world, scene);
                    });

                    _onSceneRemovedHandler = CurrentProject.World.GetOnSceneRemovedDelegate().Bind((World world, Scene scene) =>
                    {
                        SceneRemoved?.Invoke(world, scene);
                    });
                }
            });

            SetEditorViewportsEnabled(true);
        }

        public static void InitializeGame(Game game)
        {
            if (game is HyperionEditorGame)
                throw new ArgumentException("InitializeGame() shouldn't be called with an instance of HyperionEditorGame - use InitializeEditor() instead");

            _onCurrentProjectChanged?.Remove();
            _onCurrentProjectChanged = null;

            _onSceneAddedHandler?.Remove();
            _onSceneAddedHandler = game.World.GetOnSceneAddedDelegate().Bind((World world, Scene scene) =>
            {
                SceneAdded?.Invoke(world, scene);
            });

            _onSceneRemovedHandler?.Remove();
            _onSceneRemovedHandler = game.World.GetOnSceneRemovedDelegate().Bind((World world, Scene scene) =>
            {
                SceneRemoved?.Invoke(world, scene);
            });

            EngineDriver.Instance.GameInstance = game;
            GameInstance = game;

            SetEditorViewportsEnabled(false);
        }

        public static void Shutdown()
        {
            _gameLaunchedHandler?.Remove();
            _onCurrentProjectChanged?.Remove();
            _onSceneAddedHandler?.Remove();
            _onSceneRemovedHandler?.Remove();

            GameInstance = null;
            EditorGame = null;

            GC.Collect(0, GCCollectionMode.Forced, true);

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

            IntPtr assemblyGuidPtr = Marshal.AllocHGlobal(Marshal.SizeOf<Guid>());
            IntPtr assemblyPathPtr = IntPtr.Zero;

            try
            {
                string[] coreAssemblyNames = [
                    "Hyperion.NET.Shared.dll",
                    "Hyperion.NET.Runtime.dll",

                    // Loading our own dll is necessary to load HyperionEditorGame into class registry.
                    "Hyperion.Editor.dll"
                ];

                foreach (string assemblyName in coreAssemblyNames)
                {
                    string assemblyPath = Path.Combine(AppContext.BaseDirectory, assemblyName);
                    assemblyPathPtr = Marshal.StringToHGlobalAnsi(assemblyPath);

                    res = NativeInterop.InitializeAssemblyManaged(assemblyGuidPtr, IntPtr.Zero, assemblyPathPtr, /* isCoreAssembly */ 1);

                    Marshal.FreeHGlobal(assemblyPathPtr);
                    assemblyPathPtr = IntPtr.Zero;

                    if (res != (int)LoadAssemblyResult.Ok)
                    {
                        throw new Exception("Failed to initialize assembly at: " + assemblyPath + ". Error code: " + (LoadAssemblyResult)res);
                    }
                }
            }
            catch (Exception ex)
            {
                throw new Exception("Exception during Hyperion .NET runtime initialization: " + ex.Message);
            }
            finally
            {
                if (assemblyPathPtr != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(assemblyPathPtr);
                }

                if (assemblyGuidPtr != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(assemblyGuidPtr);
                }
            }
        }

        public static async Task PostToSimThread(Action action)
        {
            Game? gameInstance = GameInstance;

            if (gameInstance == null)
            {
                throw new InvalidOperationException("Game instance is not available.");
            }

            await gameInstance.PostTask(action).ConfigureAwait(false);
        }

        public static async Task<T> PostToSimThread<T>(Func<T> func)
        {
            Game? gameInstance = GameInstance;

            if (gameInstance == null)
            {
                throw new InvalidOperationException("Game instance is not available.");
            }

            return await gameInstance.PostTask(func).ConfigureAwait(false);
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
                            Logger.Log(LogType.Warn, $"EditorViewport {viewport.Id} is no longer registered - skipping addition to EditorSubsystem.");
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
            
            WeakHandle<EditorViewport> viewportWeak = new WeakHandle<EditorViewport>(viewport);

            // not launched; add handler for after launch
            _gameLaunchedHandler = EditorGame.GetOnLaunchedDelegate().Bind(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    // if set to disabled in the time between
                    if (!_editorViewportsEnabled)
                    {
                        viewportWeak.Dispose();
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

                        EditorViewport? registeredViewport = _registeredViewports.Find(v => v.Id == viewportWeak.Id);
                        if (registeredViewport == null)
                        {
                            Logger.Log(LogType.Warn, $"EditorViewport {viewportWeak.Id} is no longer registered - skipping addition to EditorSubsystem.");
                            return;
                        }
                    
                        editorSubsystem.AddViewport(registeredViewport);
                    }
                    finally
                    {
                        _lockViewports.Exit();

                        viewportWeak.Dispose();
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
                            Logger.Log(LogType.Warn, $"EditorViewport {viewport.Id} is still registered (re-added?) - skipping removal from EditorSubsystem.");
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
