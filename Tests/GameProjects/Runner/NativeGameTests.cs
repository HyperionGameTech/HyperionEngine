using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

/// A C++ game (Fixtures/NativeTest) with its own ObjectBase and AssetObject types, reflected by CodeGen, and scripts that use
/// them through the C# bindings the game's build writes
public class NativeGameTests : IClassFixture<NativeTestFixture>
{
    private readonly NativeTestFixture _fixture;
    private readonly ITestOutputHelper _output;

    public NativeGameTests(NativeTestFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    [Fact]
    public void Builds()
    {
        GameAssert.Succeeded(_fixture.Build);

        Assert.True(File.Exists(_fixture.Project.ExecutablePath), $"{_fixture.Project.ExecutablePath} was not built");
        Assert.True(File.Exists(_fixture.Project.NativeModulePath), $"{_fixture.Project.NativeModulePath} was not built");
        Assert.True(File.Exists(_fixture.Project.BindingsAssemblyPath), $"{_fixture.Project.BindingsAssemblyPath} was not built");
    }

    [Fact]
    public void Build_StampsEngineVersion()
    {
        GameAssert.Succeeded(_fixture.Build);

        Assert.True(File.Exists(_fixture.Project.EngineVersionStampPath), $"{_fixture.Project.EngineVersionStampPath} was not written");
        Assert.Equal(HarnessEnvironment.SdkVersion, File.ReadAllText(_fixture.Project.EngineVersionStampPath).Trim());
    }

    [Fact]
    public void GameTypesAndAssets_PassInEngine()
    {
        GameAssert.Passed(_fixture.WritePhase, _output);
    }

    [Fact]
    public void SavedAssets_LoadInNewSession()
    {
        GameAssert.Passed(_fixture.ReadPhase, _output);
    }

    [Fact]
    public void Bindings_DeclareGameTypes()
    {
        GameAssert.Succeeded(_fixture.Build);

        IReadOnlyDictionary<string, AssemblyType> types = AssemblyTypes.Read(_fixture.Project.BindingsAssemblyPath);

        AssertBinding(types, "NativeTestPickup", "Hyperion.ObjectBase", "PickupName", "Quantity");
        AssertBinding(types, "NativeTestMedkit", "Hyperion.NativeTestPickup", "HealAmount");
        AssertBinding(types, "NativeTestWeaponAsset", "Hyperion.AssetObject", "Damage", "FireRate", "DisplayName", "IsAutomatic", "MuzzleOffset", "Rarity", "Ammo");
        AssertBinding(types, "NativeTestNoteAsset", "Hyperion.AssetObject", "Text");
        AssertBinding(types, "NativeTestGame", "Hyperion.Game");
        AssertBinding(types, "NativeTestRarity", "System.Enum");

        AssertMethod(types, "NativeTestPickupExtensions", "GetDoubledQuantity");
        AssertMethod(types, "NativeTestMedkitExtensions", "GetTotalHealing");
    }

    [Fact]
    public void Scripts_CompileAgainstBindings()
    {
        GameAssert.Succeeded(_fixture.Build);

        GameAssert.Succeeded(_fixture.Project.BuildScripts(_fixture.Project.BindingsAssemblyPath));
    }

    // the editor loads a game module after its own classes are initialized, which initializes each class as it registers,
    // so a game class has to register after the game classes it derives from
    [Fact]
    public void CodeGen_RegistersParentsBeforeChildren()
    {
        GameAssert.Succeeded(_fixture.Build);

        string generatedSourcesPath = Path.Combine(_fixture.Project.NativeGeneratedDirectory, "GeneratedSources.cpp");
        string generatedSources = File.ReadAllText(generatedSourcesPath);

        int RegistrationIndex(string className)
        {
            int index = generatedSources.IndexOf($"s_classInit{className};", StringComparison.Ordinal);

            Assert.True(index >= 0, $"{generatedSourcesPath} doesn't register {className}");

            return index;
        }

        Assert.True(RegistrationIndex("NativeTestRarity") < RegistrationIndex("NativeTestPickup"), "enums should register before classes");
        Assert.True(RegistrationIndex("NativeTestPickup") < RegistrationIndex("NativeTestMedkit"), "NativeTestPickup should register before NativeTestMedkit, which derives from it");

        RegistrationIndex("NativeTestWeaponAsset");
        RegistrationIndex("NativeTestNoteAsset");
        RegistrationIndex("NativeTestGame");
    }

    private static void AssertBinding(IReadOnlyDictionary<string, AssemblyType> types, string className, string baseTypeName, params string[] propertyNames)
    {
        Assert.True(types.TryGetValue("Hyperion." + className, out AssemblyType? type), $"The bindings have no Hyperion.{className}");

        Assert.Equal(className, type!.ClassBindingName);
        Assert.Equal(baseTypeName, type.BaseTypeName);

        foreach (string propertyName in propertyNames)
        {
            Assert.True(type.PropertyNames.Contains(propertyName), $"Hyperion.{className} has no property {propertyName} (has: {string.Join(", ", type.PropertyNames)})");
        }
    }

    private static void AssertMethod(IReadOnlyDictionary<string, AssemblyType> types, string typeName, string methodName)
    {
        Assert.True(types.TryGetValue("Hyperion." + typeName, out AssemblyType? type), $"The bindings have no Hyperion.{typeName}");
        Assert.True(type!.MethodNames.Contains(methodName), $"Hyperion.{typeName} has no method {methodName}");
    }
}
