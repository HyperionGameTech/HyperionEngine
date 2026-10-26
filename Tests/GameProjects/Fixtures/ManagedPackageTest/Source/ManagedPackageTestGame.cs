using System.Diagnostics;
using System.Text;
using System.Text.Json;

using Hyperion;

public class ManagedPackageTestGame : Game
{
    private sealed record CheckResult(string Name, bool Passed, string? Message, double DurationMs);

    protected override void OnLaunch()
    {
        Logger.Log(LogLevel.Info, "ManagedPackageTestGame launched");

        List<CheckResult> results = new List<CheckResult>
        {
            Run("Engine_LinkedIntoExecutable", CheckEngineLinkedIntoExecutable),
            Run("GameType_HasDynamicClass", CheckGameTypeHasDynamicClass),
            Run("GameObject_CreatesNativeInstance", CheckGameObjectCreatesNativeInstance),
            Run("Script_CompiledIntoPackage", CheckScriptCompiledIntoPackage),
            Run("AssetRegistry_UsesPackageContent", CheckAssetRegistryUsesPackageContent)
        };

        int failedCount = results.Count(result => !result.Passed);

        Logger.Log(failedCount == 0 ? LogLevel.Info : LogLevel.Error, "Packaged game tests: {0} passed, {1} failed", results.Count - failedCount, failedCount);

        WriteReport(Environment.GetEnvironmentVariable("HYP_GAME_TEST_PHASE") ?? "cooked", results);

        HyperionApp.Quit();
    }

    // no hyperion library was loaded from anywhere: the engine the game runs is the one linked into it
    private void CheckEngineLinkedIntoExecutable()
    {
        foreach (ProcessModule module in Process.GetCurrentProcess().Modules)
        {
            string moduleName = module.ModuleName.ToLowerInvariant();

            Require(!moduleName.StartsWith("hyperion") && !moduleName.StartsWith("libhyperion"), $"The engine was loaded from {module.FileName}");
        }
    }

    private void CheckGameTypeHasDynamicClass()
    {
        Require(Class.TryGetDynamicClass(typeof(PackagedPickup), out Class cls), "PackagedPickup has no dynamic class");
        Require(cls.IsValid && cls.Name.ToString() == "PackagedPickup", "PackagedPickup class is invalid");
        Require(cls.GetProperty(new Name("Quantity")) != null, "PackagedPickup class has no Quantity property");
    }

    private void CheckGameObjectCreatesNativeInstance()
    {
        PackagedPickup pickup = new PackagedPickup { Quantity = 3 };

        Require(pickup.IsValid && pickup.NativeAddress != IntPtr.Zero, "PackagedPickup has no native object");
        Require(pickup.Quantity == 3, "PackagedPickup.Quantity was not kept");
    }

    // Content/Scripts is compiled into the game (HyperionIncludeScripts), so the script's type is part of this assembly
    private void CheckScriptCompiledIntoPackage()
    {
        Require(PackagedScript.Answer == 42, "PackagedScript.Answer is wrong");
        Require(typeof(PackagedScript).Assembly == typeof(ManagedPackageTestGame).Assembly, "PackagedScript is not in the game's assembly");
    }

    private void CheckAssetRegistryUsesPackageContent()
    {
        AssetRegistry assetRegistry = AssetRegistry ?? throw new InvalidOperationException("Game.AssetRegistry is null");

        string contentDirectory = Path.GetFullPath(assetRegistry.GetRootPath());
        string expected = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "Content"));

        Require(contentDirectory.StartsWith(expected, StringComparison.OrdinalIgnoreCase), $"AssetRegistry root is {contentDirectory}, not under {expected}");
    }

    private static CheckResult Run(string name, Action check)
    {
        Stopwatch stopwatch = Stopwatch.StartNew();

        try
        {
            check();

            Logger.Log(LogLevel.Info, "[PASS] {0}", name);

            return new CheckResult(name, true, null, stopwatch.Elapsed.TotalMilliseconds);
        }
        catch (Exception ex)
        {
            Logger.Log(LogLevel.Error, "[FAIL] {0}: {1}", name, ex.Message);

            return new CheckResult(name, false, ex.ToString(), stopwatch.Elapsed.TotalMilliseconds);
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }

    // the harness's GameTestReport, written without reflection-based serialization (not available under NativeAOT)
    private static void WriteReport(string phase, List<CheckResult> results)
    {
        string? resultsPath = Environment.GetEnvironmentVariable("HYP_GAME_TEST_RESULTS");

        if (string.IsNullOrEmpty(resultsPath))
        {
            return;
        }

        using MemoryStream stream = new MemoryStream();

        using (Utf8JsonWriter writer = new Utf8JsonWriter(stream, new JsonWriterOptions { Indented = true }))
        {
            writer.WriteStartObject();
            writer.WriteString("phase", phase);
            writer.WriteStartArray("tests");

            foreach (CheckResult result in results)
            {
                writer.WriteStartObject();
                writer.WriteString("name", result.Name);
                writer.WriteBoolean("passed", result.Passed);
                writer.WriteString("message", result.Message);
                writer.WriteNumber("durationMs", result.DurationMs);
                writer.WriteEndObject();
            }

            writer.WriteEndArray();
            writer.WriteEndObject();
        }

        File.WriteAllText(resultsPath, Encoding.UTF8.GetString(stream.ToArray()));
    }
}

[ClassBinding(IsDynamic = true)]
public class PackagedPickup : ObjectBase
{
    [HypField(Serialize = true)]
    public int Quantity = 0;
}
