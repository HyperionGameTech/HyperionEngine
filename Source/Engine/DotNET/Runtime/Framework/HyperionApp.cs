using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [NoNativeClass]
    public struct HyperionAppOptions
    {
        public List<string> EngineArgs { get; } = new();
        public string? BaseDirectory { get; set; }

        public string? ContentDirectory { get; set; }
        public string? CacheDirectory { get; set; }

        public bool UseCookedContent { get; set; } = false;
        public bool UseGlobalConfigArgs { get; set; } = false;

        public bool SinglePlayer { get; set; } = true;
        public bool Headless { get; set; } = false;
        public int MainThreadHz { get; set; } = 120;

        public HyperionAppOptions()
        {
        }
    }

    /// C# bindings for the native Hyp_* C functions, for managing global engine state.
    public static class HyperionApp
    {
        private static readonly List<string> s_coreAssemblyPaths = new();

        public static Game? CurrentGame { get; private set; }

        public static int Run<TGame>(string[] args, HyperionAppOptions? appOptions = null) where TGame : Game, new()
        {
            HyperionAppOptions options = appOptions ?? new HyperionAppOptions();

            if (CurrentGame != null)
            {
                throw new InvalidOperationException("HyperionApp is already running");
            }

            s_coreAssemblyPaths.Clear();
            s_coreAssemblyPaths.Add(typeof(ObjectBase).Assembly.Location);
            s_coreAssemblyPaths.Add(typeof(Game).Assembly.Location);

            string gameAssemblyPath = typeof(TGame).Assembly.Location;

            if (!s_coreAssemblyPaths.Contains(gameAssemblyPath))
            {
                s_coreAssemblyPaths.Add(gameAssemblyPath);
            }

            unsafe
            {
                Hyp_SetInitFromManagedCallback(&InitFromManagedCallback);
            }

            if (!Initialize(BuildEngineArgs(typeof(TGame).Assembly, args, options)))
            {
                Console.Error.WriteLine("Hyperion: failed to initialize the engine");

                return 1;
            }

            TGame game = new TGame();
            CurrentGame = game;

            EngineDriver.Instance.GameInstance = game;

            if (Hyp_LaunchThreads() == 0)
            {
                Console.Error.WriteLine("Hyperion: failed to launch engine threads");

                Shutdown(game);

                return 1;
            }

            RunMainLoop(options.MainThreadHz);

            Shutdown(game);

            return 0;
        }

        public static void Quit()
        {
            Hyp_RequestQuit();
        }

        private static List<string> BuildEngineArgs(Assembly gameAssembly, string[] args, HyperionAppOptions options)
        {
            List<string> engineArgs = new List<string>
            {
                Environment.ProcessPath ?? gameAssembly.Location,
                "--detached"
            };

            if (options.UseGlobalConfigArgs)
            {
                engineArgs.AddRange(options.EngineArgs);
                engineArgs.AddRange(args);

                return engineArgs;
            }

            string? baseDirectory = options.BaseDirectory ?? FindAssemblyMetadata(gameAssembly, "HyperionBaseDir");

            if (!string.IsNullOrEmpty(baseDirectory) && Directory.Exists(baseDirectory))
            {
                engineArgs.Add("--basedir=" + Path.TrimEndingDirectorySeparator(Path.GetFullPath(baseDirectory)));
            }

            if (!string.IsNullOrEmpty(options.ContentDirectory))
            {
                engineArgs.Add("--contentdir=" + Path.GetFullPath(options.ContentDirectory, AppContext.BaseDirectory));
            }

            if (!string.IsNullOrEmpty(options.CacheDirectory))
            {
                engineArgs.Add("--cachedir=" + Path.GetFullPath(options.CacheDirectory, AppContext.BaseDirectory));
            }

            if (!options.UseCookedContent)
            {
                engineArgs.Add("--cooked=false");
            }

            if (options.SinglePlayer)
            {
                engineArgs.Add("--singleplayer");
            }

            if (options.Headless)
            {
                engineArgs.Add("--headless");
            }

            engineArgs.AddRange(options.EngineArgs);
            engineArgs.AddRange(args);

            return engineArgs;
        }

        private static string? FindAssemblyMetadata(Assembly assembly, string key)
        {
            foreach (AssemblyMetadataAttribute attribute in assembly.GetCustomAttributes<AssemblyMetadataAttribute>())
            {
                if (attribute.Key == key)
                {
                    return attribute.Value;
                }
            }

            return null;
        }

        private static bool Initialize(List<string> engineArgs)
        {
            int argc = engineArgs.Count;
            IntPtr[] argPtrs = new IntPtr[argc];
            IntPtr argv = IntPtr.Zero;

            try
            {
                for (int i = 0; i < argc; i++)
                {
                    argPtrs[i] = Marshal.StringToCoTaskMemUTF8(engineArgs[i]);
                }

                argv = Marshal.AllocHGlobal(IntPtr.Size * argc);

                for (int i = 0; i < argc; i++)
                {
                    Marshal.WriteIntPtr(argv, i * IntPtr.Size, argPtrs[i]);
                }

                return Hyp_Initialize(argc, argv) != 0;
            }
            finally
            {
                foreach (IntPtr argPtr in argPtrs)
                {
                    if (argPtr != IntPtr.Zero)
                    {
                        Marshal.FreeCoTaskMem(argPtr);
                    }
                }

                if (argv != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(argv);
                }
            }
        }

        private static void RunMainLoop(int mainThreadHz)
        {
            long ticksPerFrame = Stopwatch.Frequency / Math.Max(1, mainThreadHz);

            Stopwatch stopwatch = Stopwatch.StartNew();

            while (Hyp_IsQuitRequested() == 0)
            {
                long frameStart = stopwatch.ElapsedTicks;

                Hyp_MainThreadUpdate();

                long remainingMs = (ticksPerFrame - (stopwatch.ElapsedTicks - frameStart)) * 1000 / Stopwatch.Frequency;

                if (remainingMs > 0)
                {
                    Thread.Sleep((int)remainingMs);
                }
            }
        }

        private static void Shutdown(Game game)
        {
            try
            {
                // Game shutdown touches the World, which belongs to the sim thread
                if (!SimThread.PostTask(() => game.Shutdown(true)).Wait(TimeSpan.FromSeconds(10)))
                {
                    Logger.Log(LogLevel.Warning, "Timed out waiting for the game to shut down on the sim thread");
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception shutting down game: {0}", ex);
            }

            CurrentGame = null;

            ObjectBase.IsEngineShuttingDown = true;

            Hyp_Shutdown();
        }

        [UnmanagedCallersOnly]
        private static unsafe void InitFromManagedCallback(ManagedDelegates* pManagedDelegates)
        {
            int result = NativeInterop.InitializeRuntimeManaged();

            if (result != (int)LoadAssemblyResult.Ok)
            {
                throw new Exception("Failed to initialize the Hyperion .NET runtime. Error code: " + (LoadAssemblyResult)result);
            }

            pManagedDelegates->initializeAssembly = (delegate* unmanaged<IntPtr, IntPtr, IntPtr, int, int>)&NativeInterop.InitializeAssembly;
            pManagedDelegates->unloadAssembly = (delegate* unmanaged<IntPtr, IntPtr, void>)&NativeInterop.UnloadAssembly;

            IntPtr assemblyGuidPtr = Marshal.AllocHGlobal(Marshal.SizeOf<Guid>());

            try
            {
                foreach (string assemblyPath in s_coreAssemblyPaths)
                {
                    IntPtr assemblyPathPtr = Marshal.StringToCoTaskMemUTF8(assemblyPath);

                    try
                    {
                        result = NativeInterop.InitializeAssemblyManaged(assemblyGuidPtr, IntPtr.Zero, assemblyPathPtr, /* isCoreAssembly */ 1);
                    }
                    finally
                    {
                        Marshal.FreeCoTaskMem(assemblyPathPtr);
                    }

                    if (result != (int)LoadAssemblyResult.Ok)
                    {
                        throw new Exception("Failed to initialize assembly at: " + assemblyPath + ". Error code: " + (LoadAssemblyResult)result);
                    }
                }
            }
            finally
            {
                Marshal.FreeHGlobal(assemblyGuidPtr);
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private unsafe struct ManagedDelegates
        {
            public delegate* unmanaged<int> initializeRuntime;
            public delegate* unmanaged<IntPtr, IntPtr, IntPtr, int, int> initializeAssembly;
            public delegate* unmanaged<IntPtr, IntPtr, void> unloadAssembly;
        }

        [DllImport("hyperion")]
        private static extern int Hyp_Initialize(int argc, IntPtr argv);

        [DllImport("hyperion")]
        private static extern int Hyp_LaunchThreads();

        [DllImport("hyperion")]
        private static extern void Hyp_MainThreadUpdate();

        [DllImport("hyperion")]
        private static extern int Hyp_IsQuitRequested();

        [DllImport("hyperion")]
        private static extern void Hyp_RequestQuit();

        [DllImport("hyperion")]
        private static extern void Hyp_Shutdown();

        [DllImport("hyperion")]
        private static extern unsafe void Hyp_SetInitFromManagedCallback(delegate* unmanaged<ManagedDelegates*, void> callback);
    }
}
