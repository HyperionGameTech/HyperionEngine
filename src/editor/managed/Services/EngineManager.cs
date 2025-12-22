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
        private static DelegateHandler? _onCurrentProjectChanged;
        private static DelegateHandler? _gameLaunchedHandler;
        private static List<EditorViewport> _registeredViewports = new List<EditorViewport>();
        private static Lock _lockViewports = new Lock();
        private static bool _editorViewportsEnabled = true;

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

        public static void InitializeEditor()
        {
            EditorGame ??= new HyperionEditorGame();

            if (EditorGame.IsLaunched())
            {
                _lockViewports.Enter();
                try
                {
                    EditorSubsystem? editorSubsystem = EditorGame.EditorSubsystem;
                    if (editorSubsystem == null)
                    {
                        throw new InvalidOperationException("EditorSubsystem is not initialized!");
                    }

                    foreach (EditorViewport vp in _registeredViewports)
                    {
                        editorSubsystem.AddViewport(vp);
                    }
                }
                finally
                {
                    _lockViewports.Exit();
                }
            }

            GameInstance = EditorGame;
            
            EngineDriver.Instance.GameInstance = EditorGame;

            EditorState editorState = EditorState.Instance;
            Debug.Assert(editorState != null, "Failed to get EditorState instance");

            CurrentProject = editorState.CurrentProject;

            _onCurrentProjectChanged?.Remove();
            _onCurrentProjectChanged = editorState.GetOnCurrentProjectChangedDelegate().Bind((EditorProject newProject) =>
            {
                CurrentProject = newProject;

                Logger.Log(LogType.Info, "Current project changed to: " + (CurrentProject != null ? CurrentProject.Name : "null"));
            });

            SetEditorViewportsEnabled(true);
        }

        public static void InitializeGame(Game game)
        {
            if (game is HyperionEditorGame)
                throw new ArgumentException("InitializeGame() shouldn't be called with an instance of HyperionEditorGame - use InitializeEditor() instead");

            _onCurrentProjectChanged?.Remove();
            _onCurrentProjectChanged = null;
            
            EngineDriver.Instance.GameInstance = game;
            GameInstance = game;

            SetEditorViewportsEnabled(false);
        }

        public static void Shutdown()
        {
            EditorGame = null;

            NativeBindings.Hyp_Shutdown();
            IsInitialized = false;
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
                    "Hyperion.NET.Scripting.dll",

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
                        EditorViewport? removedViewport = _registeredViewports.Find(v => v.Id == viewport.Id);
                        if (removedViewport != null)
                        {
                            Logger.Log(LogType.Warn, $"EditorViewport {viewport.Id} is still registered (re-added?) - skipping removal from EditorSubsystem.");
                            return;
                        }

                        editorSubsystem.RemoveViewport(removedViewport);
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
