using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Hyperion;

namespace Hyperion.Editor
{
    public static class EngineManager
    {

        public static bool IsInitialized { get; private set; }
        public static HyperionEditorGame? GameInstance { get; private set; }

        public static EditorProject? CurrentProject { get; private set; }

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
            if (GameInstance != null)
                return;

            GameInstance = new HyperionEditorGame();

            if (EngineDriver.Instance.SetGame(GameInstance) == 0)
            {
                throw new Exception("Failed to launch HyperionEditor instance");
            }

            World editorWorld = GameInstance.World;

            EditorState editorState = EditorState.Instance;
            Assert.Throw(editorState != null, "Failed to get EditorState instance");

            CurrentProject = editorState.CurrentProject;

            ScriptableDelegate del = editorState.GetOnCurrentProjectChangedDelegate();
            del.Bind((EditorProject newProject) =>
            {
                CurrentProject = newProject;

                Logger.Log(LogType.Info, "Current project changed to: " + (CurrentProject != null ? CurrentProject.Name : "null"));
            }).Detach();
        }

        public static void Shutdown()
        {
            GameInstance = null;

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
    }
}
