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

        public bool Headless { get; set; } = false;
        public int MainThreadHz { get; set; } = 120;

        public Assembly? GameAssembly { get; set; }

        public HyperionAppOptions()
        {
        }
    }

    /// C# bindings for the native Hyp_* C functions, for managing global engine state.
    public static class HyperionApp
    {
        private static readonly List<Assembly> s_coreAssemblies = new();
        private static Assembly? s_gameAssembly;
        private static readonly Dictionary<string, Assembly> s_pluginAssemblies = new();

        public static Game? CurrentGame { get; private set; }

        public static int Run<TGame>(string[] args, HyperionAppOptions? appOptions = null) where TGame : Game, new()
        {
            HyperionAppOptions options = appOptions ?? new HyperionAppOptions();

            if (CurrentGame != null)
            {
                throw new InvalidOperationException("HyperionApp is already running");
            }

            Assembly gameAssembly = options.GameAssembly ?? typeof(TGame).Assembly;

            InitializeHosted(gameAssembly);

            if (!Initialize(BuildEngineArgs(gameAssembly, args, options)))
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

        public static void InitializeHosted(Assembly gameAssembly)
        {
            s_gameAssembly = gameAssembly;

            s_coreAssemblies.Clear();
            s_coreAssemblies.Add(typeof(ObjectBase).Assembly);
            s_coreAssemblies.Add(typeof(Game).Assembly);

            RegisterPluginAssemblies(gameAssembly);

            if (!s_coreAssemblies.Contains(gameAssembly))
            {
                s_coreAssemblies.Add(gameAssembly);
            }

            unsafe
            {
                Hyp_SetInitFromManagedCallback(&InitFromManagedCallback);
            }
        }

        // the plugin assemblies (Hyperion.Plugin.<Module>) hostAssembly is built against; each is initialized once the engine loads its module
        // (PluginModuleLoadedCallback), as its classes only exist from then on
        public static void RegisterPluginAssemblies(Assembly hostAssembly)
        {
            foreach (HypAssemblyDependencyAttribute dependency in hostAssembly.GetCustomAttributes<HypAssemblyDependencyAttribute>())
            {
                Assembly assembly = dependency.RegistryType.Assembly;

                s_pluginAssemblies[assembly.GetName().Name!] = assembly;
            }
        }

        public static unsafe delegate* unmanaged<IntPtr, void> PluginModuleLoadedCallback => &OnPluginModuleLoaded;

        [UnmanagedCallersOnly]
        private static void OnPluginModuleLoaded(IntPtr moduleNamePtr)
        {
            string assemblyName = EngineAssemblies.PluginPrefix + (Marshal.PtrToStringUTF8(moduleNamePtr) ?? string.Empty);

            if (!s_pluginAssemblies.TryGetValue(assemblyName, out Assembly? assembly))
            {
                return;
            }

            int result = NativeInterop.InitializeLoadedAssembly(assembly, isCoreAssembly: true);

            if (result != (int)LoadAssemblyResult.Ok)
            {
                Logger.Log(LogLevel.Error, "Failed to initialize plugin assembly {0}: {1}", assemblyName, (LoadAssemblyResult)result);
            }
        }

        public static IntPtr StartHostedGame(Game game)
        {
            CurrentGame = game;

            return game.NativeAddress;
        }

        private static List<string> BuildEngineArgs(Assembly gameAssembly, string[] args, HyperionAppOptions options)
        {
            List<string> engineArgs = new List<string>
            {
                Environment.ProcessPath ?? AppContext.BaseDirectory,
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
            pManagedDelegates->pluginModuleLoaded = PluginModuleLoadedCallback;

            foreach (Assembly assembly in s_coreAssemblies)
            {
                result = NativeInterop.InitializeLoadedAssembly(assembly, isCoreAssembly: true);

                if (result != (int)LoadAssemblyResult.Ok)
                {
                    throw new Exception("Failed to initialize assembly " + assembly.GetName().Name + ". Error code: " + (LoadAssemblyResult)result);
                }
            }

            // the game's scripts are compiled into it
            if (s_gameAssembly != null)
            {
                NativeInterop.SetHostedScriptAssembly(s_gameAssembly);
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private unsafe struct ManagedDelegates
        {
            public delegate* unmanaged<IntPtr, IntPtr, IntPtr, int, int> initializeAssembly;
            public delegate* unmanaged<IntPtr, IntPtr, void> unloadAssembly;
            public delegate* unmanaged<IntPtr, void> pluginModuleLoaded;
        }

        ////////////////////

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
