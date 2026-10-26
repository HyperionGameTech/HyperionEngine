using System.Runtime.InteropServices;

using Hyperion;

public static unsafe class Program
{
    public static int Main(string[] args)
    {
        if (args.Length < 3)
        {
            Console.Error.WriteLine("Usage: EditorTestHost create <project file> <managed|native> | simulate <project directory> <frames> <results file> | play <project directory> <frames> <Standalone|Client|DedicatedServer> <host> <port> <expected clients> <results file>");

            return 2;
        }

        string command = args[0];
        int numCommandArgs = command switch
        {
            "simulate" => 4,
            "play" => 8,
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
            "create" => EditorTestHost_CreateProject(args[1], args[2] == "managed" ? 1 : 0),
            "simulate" => EditorTestHost_Simulate(args[1], int.Parse(args[2]), args[3]),
            "play" => EditorTestHost_Play(args[1], int.Parse(args[2]), args[3], args[4], int.Parse(args[5]), int.Parse(args[6]), args[7]),
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

        foreach (System.Reflection.Assembly assembly in new[] { typeof(ObjectBase).Assembly, typeof(Game).Assembly }.Distinct())
        {
            result = NativeInterop.InitializeLoadedAssembly(assembly, isCoreAssembly: true);

            if (result != (int)LoadAssemblyResult.Ok)
            {
                throw new Exception("Failed to initialize assembly " + assembly.GetName().Name + ". Error code: " + (LoadAssemblyResult)result);
            }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ManagedDelegates
    {
        public delegate* unmanaged<IntPtr, IntPtr, IntPtr, int, int> initializeAssembly;
        public delegate* unmanaged<IntPtr, IntPtr, void> unloadAssembly;
    }

    [DllImport("hyperion")]
    private static extern void Hyp_SetInitFromManagedCallback(delegate* unmanaged<ManagedDelegates*, void> callback);

    [DllImport("hyperion")]
    private static extern int Hyp_Initialize(int argc, IntPtr argv);

    [DllImport("hyperion")]
    private static extern void Hyp_Shutdown();

    [DllImport("hyperion", CharSet = CharSet.Ansi)]
    private static extern int EditorTestHost_CreateProject([MarshalAs(UnmanagedType.LPUTF8Str)] string projectFilepath, int isManaged);

    [DllImport("hyperion", CharSet = CharSet.Ansi)]
    private static extern int EditorTestHost_Simulate([MarshalAs(UnmanagedType.LPUTF8Str)] string projectDirectory, int numFrames, [MarshalAs(UnmanagedType.LPUTF8Str)] string resultsPath);

    [DllImport("hyperion", CharSet = CharSet.Ansi)]
    private static extern int EditorTestHost_Play(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string projectDirectory,
        int numFrames,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string netMode,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string host,
        int port,
        int expectedClients,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string resultsPath);
}
