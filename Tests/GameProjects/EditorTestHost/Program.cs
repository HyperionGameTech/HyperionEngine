using System.Runtime.InteropServices;

using Hyperion;

public static unsafe class Program
{
    public static int Main(string[] args)
    {
        if (args.Length < 3)
        {
            Console.Error.WriteLine("Usage: EditorTestHost create <project file> <managed|native> <disabled plugins, comma separated, or -> | simulate <project directory> <frames> <results file> | switch <project directories, ';' separated> <frames> <results file> | play <project directory> <frames> <Standalone|Client|DedicatedServer> <host> <port> <expected clients> <results file> | editor-tests <filter, or -> <list|run> <results file>");

            return 2;
        }

        string command = args[0];
        int numCommandArgs = command switch
        {
            "create" => 4,
            "simulate" => 4,
            "switch" => 4,
            "play" => 8,
            "editor-tests" => 4,
            _ => 3
        };

        List<string> engineArgs = new List<string>
        {
            Environment.ProcessPath ?? AppContext.BaseDirectory,
            "--detached",
            "--headless",
            "--editor",
            "--singleplayer"
        };

        engineArgs.AddRange(args.Skip(numCommandArgs));

        Hyp_SetInitFromManagedCallback(&InitFromManagedCallback);

        if (!Initialize(engineArgs))
        {
            Console.Error.WriteLine("EditorTestHost: failed to initialize the engine");

            return 1;
        }

        int succeeded = command switch
        {
            "create" => EditorTestHost_CreateProject(args[1], args[2] == "managed" ? 1 : 0, args[3] == "-" ? "" : args[3]),
            "simulate" => EditorTestHost_Simulate(args[1], int.Parse(args[2]), args[3]),
            "switch" => EditorTestHost_Switch(args[1], int.Parse(args[2]), args[3]),
            "play" => EditorTestHost_Play(args[1], int.Parse(args[2]), args[3], args[4], int.Parse(args[5]), int.Parse(args[6]), args[7]),
            "editor-tests" => EditorTestHost_RunEditorTests(args[1] == "-" ? "" : args[1], args[2] == "list" ? 1 : 0, args[3]),
            _ => 0
        };

        ObjectBase.IsEngineShuttingDown = true;

        Hyp_Shutdown();

        return succeeded != 0 ? 0 : 1;
    }

    private static bool Initialize(List<string> engineArgs)
    {
        IntPtr[] argPtrs = engineArgs.Select(arg => Marshal.StringToCoTaskMemUTF8(arg)).ToArray();

        try
        {
            fixed (IntPtr* argv = argPtrs)
            {
                return Hyp_Initialize(argPtrs.Length, (IntPtr)argv) != 0;
            }
        }
        finally
        {
            foreach (IntPtr argPtr in argPtrs)
            {
                Marshal.FreeCoTaskMem(argPtr);
            }
        }
    }

    // the editor's (EngineManager.InitFromManagedCallback), less Hyperion.Editor.dll, which is the editor's UI
    [UnmanagedCallersOnly]
    private static void InitFromManagedCallback(ManagedDelegates* pManagedDelegates)
    {
        int result = NativeInterop.InitializeRuntimeManaged();

        if (result != (int)LoadAssemblyResult.Ok)
        {
            throw new Exception("Failed to initialize the Hyperion .NET runtime. Error code: " + (LoadAssemblyResult)result);
        }

        pManagedDelegates->initializeAssembly = (delegate* unmanaged<IntPtr, IntPtr, IntPtr, int, int>)&NativeInterop.InitializeAssembly;
        pManagedDelegates->unloadAssembly = (delegate* unmanaged<IntPtr, IntPtr, void>)&NativeInterop.UnloadAssembly;
        pManagedDelegates->pluginModuleLoaded = &OnPluginModuleLoaded;

        foreach (System.Reflection.Assembly assembly in new[] { typeof(ObjectBase).Assembly, typeof(Game).Assembly }.Distinct())
        {
            result = NativeInterop.InitializeLoadedAssembly(assembly, isCoreAssembly: true);

            if (result != (int)LoadAssemblyResult.Ok)
            {
                throw new Exception("Failed to initialize assembly " + assembly.GetName().Name + ". Error code: " + (LoadAssemblyResult)result);
            }
        }
    }

    // the editor's (EngineManager.OnPluginModuleLoaded), less editor modules, whose C# is the editor's UI
    [UnmanagedCallersOnly]
    private static void OnPluginModuleLoaded(IntPtr moduleNamePtr)
    {
        string moduleName = Marshal.PtrToStringUTF8(moduleNamePtr) ?? string.Empty;
        string assemblyPath = Path.Combine(AppContext.BaseDirectory, EngineAssemblies.PluginPrefix + moduleName + ".dll");

        if (moduleName.EndsWith("Editor") || !File.Exists(assemblyPath))
        {
            return;
        }

        IntPtr assemblyGuidPtr = Marshal.AllocHGlobal(Marshal.SizeOf<Guid>());
        IntPtr assemblyPathPtr = Marshal.StringToCoTaskMemUTF8(assemblyPath);

        try
        {
            int result = NativeInterop.InitializeAssemblyManaged(assemblyGuidPtr, IntPtr.Zero, assemblyPathPtr, /* isCoreAssembly */ 1, out Exception? error);

            if (result != (int)LoadAssemblyResult.Ok)
            {
                Console.Error.WriteLine($"EditorTestHost: failed to initialize plugin assembly {assemblyPath}: {(LoadAssemblyResult)result} {error?.Message}");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(assemblyPathPtr);
            Marshal.FreeHGlobal(assemblyGuidPtr);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ManagedDelegates
    {
        public delegate* unmanaged<IntPtr, IntPtr, IntPtr, int, int> initializeAssembly;
        public delegate* unmanaged<IntPtr, IntPtr, void> unloadAssembly;
        public delegate* unmanaged<IntPtr, void> pluginModuleLoaded;
    }

    [DllImport("hyperion")]
    private static extern void Hyp_SetInitFromManagedCallback(delegate* unmanaged<ManagedDelegates*, void> callback);

    [DllImport("hyperion")]
    private static extern int Hyp_Initialize(int argc, IntPtr argv);

    [DllImport("hyperion")]
    private static extern void Hyp_Shutdown();

    [DllImport("hyperion", CharSet = CharSet.Ansi)]
    private static extern int EditorTestHost_CreateProject([MarshalAs(UnmanagedType.LPUTF8Str)] string projectFilepath, int isManaged, [MarshalAs(UnmanagedType.LPUTF8Str)] string disabledPlugins);

    [DllImport("hyperion", CharSet = CharSet.Ansi)]
    private static extern int EditorTestHost_Simulate([MarshalAs(UnmanagedType.LPUTF8Str)] string projectDirectory, int numFrames, [MarshalAs(UnmanagedType.LPUTF8Str)] string resultsPath);

    [DllImport("hyperion", CharSet = CharSet.Ansi)]
    private static extern int EditorTestHost_Switch([MarshalAs(UnmanagedType.LPUTF8Str)] string projectDirectories, int numFrames, [MarshalAs(UnmanagedType.LPUTF8Str)] string resultsPath);

    [DllImport("hyperion", CharSet = CharSet.Ansi)]
    private static extern int EditorTestHost_Play(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string projectDirectory,
        int numFrames,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string netMode,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string host,
        int port,
        int expectedClients,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string resultsPath);

    [DllImport("hyperion", CharSet = CharSet.Ansi)]
    private static extern int EditorTestHost_RunEditorTests([MarshalAs(UnmanagedType.LPUTF8Str)] string filter, int listOnly, [MarshalAs(UnmanagedType.LPUTF8Str)] string resultsPath);
}
