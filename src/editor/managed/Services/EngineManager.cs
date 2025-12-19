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
        private static ConcurrentDictionary<ObjIdBase, EditorViewport> _registeredViewports = new ConcurrentDictionary<ObjIdBase, EditorViewport>();

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
                "-RenderOnMainThread=false",
                "-Editor"
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
        }

        public static void InitializeGame(Game game)
        {
            if (game is HyperionEditorGame)
                throw new ArgumentException("InitializeGame() shouldn't be called with an instance of HyperionEditorGame - use InitializeEditor() instead");

            _onCurrentProjectChanged?.Remove();
            _onCurrentProjectChanged = null;
            
            EngineDriver.Instance.GameInstance = game;
            GameInstance = game;
        }

        public static void Shutdown()
        {
            EditorGame = null;

            NativeBindings.Hyp_Shutdown();
            IsInitialized = false;
        }

        [UnmanagedCallersOnly]
        private static void InitFromManagedCallback()
        {
            // Initialize Hyperion.NET runtime
            int res = NativeInterop.InitializeRuntimeManaged();
            if (res != (int)LoadAssemblyResult.Ok)
            {
                throw new Exception("Failed to initialize Hyperion .NET runtime from managed code. Error code: " + (LoadAssemblyResult)res);
            }

            IntPtr assemblyGuidPtr = Marshal.AllocHGlobal(Marshal.SizeOf<Guid>());
            IntPtr assemblyPathStringPtr = IntPtr.Zero;

            try
            {
                string[] coreAssemblyNames = [
                    "Hyperion.NET.Shared.dll",
                    "Hyperion.NET.Runtime.dll",
                    "Hyperion.Editor.dll"
                ];

                foreach (string assemblyName in coreAssemblyNames)
                {
                    assemblyPathStringPtr = Marshal.StringToHGlobalAnsi(System.IO.Path.Combine(AppContext.BaseDirectory, assemblyName));

                    res = NativeInterop.InitializeAssemblyManaged(assemblyGuidPtr, IntPtr.Zero, assemblyPathStringPtr, /* isCoreAssembly */ 1);

                    Marshal.FreeHGlobal(assemblyPathStringPtr);
                    assemblyPathStringPtr = IntPtr.Zero;

                    if (res != (int)LoadAssemblyResult.Ok)
                    {
                        throw new Exception("Failed to initialize Hyperion.NET.Shared assembly. Error code: " + (LoadAssemblyResult)res);
                    }
                }
            }
            catch (Exception ex)
            {
                throw new Exception("Exception during Hyperion .NET runtime initialization: " + ex.Message);
            }
            finally
            {
                if (assemblyPathStringPtr != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(assemblyPathStringPtr);
                }

                if (assemblyGuidPtr != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(assemblyGuidPtr);
                }
            }
        }

        public static async Task PostToGameThread(Action action)
        {
            Game? gameInstance = GameInstance;

            if (gameInstance == null)
            {
                throw new InvalidOperationException("Game instance is not available.");
            }

            await gameInstance.PostTask(action).ConfigureAwait(false);
        }

        public static async Task<T> PostToGameThread<T>(Func<T> func)
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
            if (!_registeredViewports.TryAdd(viewport.Id, viewport))
            {
                Logger.Log(LogType.Warn, "EditorViewport is already registered.");
                return;
            }

            if (EditorGame == null)
            {
                return;
            }

            if (EditorGame.IsLaunched())
            {
                _ = EngineManager.PostToGameThread(() =>
                {
                    EditorSubsystem? editorSubsystem = EditorGame.EditorSubsystem;
                    if (editorSubsystem == null)
                    {
                        throw new InvalidOperationException("EditorSubsystem is not initialized!");
                    }

                    if (!_registeredViewports.ContainsKey(viewport.Id))
                    {
                        Logger.Log(LogType.Warn, $"EditorViewport {viewport.Id} is no longer registered - skipping addition to EditorSubsystem.");
                        return;
                    }

                    editorSubsystem.AddViewport(viewport);
                });

                return;
            }
            
            WeakHandle<EditorViewport> viewportWeak = new WeakHandle<EditorViewport>(viewport);

            // not launched; add handler for after launch
            _gameLaunchedHandler = EditorGame.GetOnLaunchedDelegate().Bind(() =>
            {
                _ = EngineManager.PostToGameThread(() =>
                {
                    try
                    {
                        EditorSubsystem? editorSubsystem = EditorGame.EditorSubsystem;
                        if (editorSubsystem == null)
                        {
                            throw new InvalidOperationException("EditorSubsystem is not initialized!");
                        }

                        EditorViewport? registeredViewport = null;
                        if (!_registeredViewports.TryGetValue(viewportWeak.Id, out registeredViewport))
                        {
                            Logger.Log(LogType.Warn, $"EditorViewport {viewportWeak.Id} is no longer registered - skipping addition to EditorSubsystem.");
                            return;
                        }

                        editorSubsystem.AddViewport(registeredViewport);
                    }
                    finally
                    {
                        viewportWeak.Dispose();
                    }
                });
            });
        }

        public static void UnregisterViewport(EditorViewport viewport, bool removeFromList = true)
        {
            EditorViewport? removedViewport = null;

            if (removeFromList)
            {
                if (!_registeredViewports.TryRemove(viewport.Id, out removedViewport))
                {
                    return;
                }

                if (removedViewport == null)
                {
                    return;
                }
            }
            else
            {
                removedViewport = viewport;
            }

            if (EditorGame == null)
            {
                return;
            }
            
            if (EditorGame.IsLaunched())
            {
                _ = EngineManager.PostToGameThread(() =>
                {
                    EditorSubsystem? editorSubsystem = EditorGame.EditorSubsystem;
                    if (editorSubsystem == null)
                    {
                        throw new InvalidOperationException("EditorSubsystem is not initialized!");
                    }

                    editorSubsystem.RemoveViewport(removedViewport);
                });
            }
        }

        public static void DisableAllViewports()
        {
            if (EditorGame == null)
            {
                return;
            }

        }
    }
}
