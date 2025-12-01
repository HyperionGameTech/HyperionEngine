using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Hyperion;

namespace Hyperion.Editor
{
    public static class EngineManager
    {
        private static bool s_isInitialized = false;
        private static bool s_isGameInitialized = false;
        private static Game? s_gameInstance = null;
        private static EditorProject? s_currentProject = null;

        public static bool IsInitialized => s_isInitialized;
        public static Game? GameInstance => s_gameInstance;

        public static EditorProject? CurrentProject
        {
            get => s_currentProject;
        }

        public static void Initialize()
        {
            if (s_isInitialized) return;

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
                "-RenderOnMainThread",
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
                throw new Exception("Exception during Hyperion Engine initialization: " + ex.Message);
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

            s_isInitialized = true;
        }

        public static void InitializeEditor()
        {
            if (s_isGameInitialized) return;

            s_gameInstance = new HyperionEditorGame();

            if (NativeBindings.Hyp_LaunchGame(s_gameInstance.NativeAddress) == 0)
            {
                throw new Exception("Failed to launch HyperionEditor instance");
            }

            World editorWorld = s_gameInstance.World;

            EditorState editorState = EditorState.Instance;
            Assert.Throw(editorState != null, "Failed to get EditorState instance");

            s_currentProject = editorState.CurrentProject;

            ScriptableDelegate del = editorState.GetOnCurrentProjectChangedDelegate();
            del.Bind((EditorProject newProject) =>
            {
                s_currentProject = newProject;

                Logger.Log(LogType.Info, "Current project changed to: " + (s_currentProject != null ? s_currentProject.Name : "null"));
            }).Detach();

            s_isGameInitialized = true;
        }

        public static void Shutdown()
        {
            s_gameInstance = null;

            NativeBindings.Hyp_Shutdown();
            s_isInitialized = false;
            s_isGameInitialized = false;
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
    }
}
