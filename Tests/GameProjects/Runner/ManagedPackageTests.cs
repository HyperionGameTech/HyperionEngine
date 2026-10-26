using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

/// Packages Fixtures/ManagedPackageTest, a C# game, the way the editor's Package Game does: cooked, then NativeAOT-compiled with the
/// static shipping engine linked into the executable. The packaged game runs its checks in-engine (see ManagedPackageTestGame.cs)
public sealed class ManagedPackageFixture
{
    private readonly Lazy<GameProject> _project;
    private readonly Lazy<ProcessResult> _cook;
    private readonly Lazy<ProcessResult> _package;
    private readonly Lazy<GameRunResult> _packagedRun;

    public ManagedPackageFixture()
    {
        _project = new Lazy<GameProject>(() => GameProject.Create("ManagedPackageTest", GameProjectLanguage.Managed, fixtureName: "ManagedPackageTest"));
        _cook = new Lazy<ProcessResult>(() => Project.Cook(PackageDirectory));
        _package = new Lazy<ProcessResult>(() =>
        {
            Assert.True(Cook.Output.Contains("Blob storage cook complete."), $"The cook didn't complete.\n{Cook.Summary}");

            return Project.PublishManagedPackage(PackageDirectory);
        });
        _packagedRun = new Lazy<GameRunResult>(() =>
        {
            GameAssert.Succeeded(Package);

            return Project.RunPackaged(PackageDirectory, "cooked");
        });
    }

    public GameProject Project => _project.Value;

    public string PackageDirectory => Path.Combine(Project.Directory, "Packaged");

    public ProcessResult Cook => _cook.Value;

    public ProcessResult Package => _package.Value;

    public GameRunResult PackagedRun => _packagedRun.Value;
}

public class ManagedPackageTests : IClassFixture<ManagedPackageFixture>
{
    private readonly ManagedPackageFixture _fixture;
    private readonly ITestOutputHelper _output;

    public ManagedPackageTests(ManagedPackageFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    [Fact]
    public void Package_Publishes()
    {
        GameAssert.Succeeded(_fixture.Package);

        string executablePath = Path.Combine(_fixture.PackageDirectory, _fixture.Project.Name + HarnessEnvironment.ExecutableExtension);
        Assert.True(File.Exists(executablePath), $"{executablePath} was not published");
    }

    [Fact]
    public void Package_LinksShippingEngineStatically()
    {
        GameAssert.Succeeded(_fixture.Package);
        GameAssert.NoEngineLibraries(_fixture.PackageDirectory);
    }

    [Fact]
    public void PackagedGame_PassesInEngine()
    {
        GameAssert.Passed(_fixture.PackagedRun, _output);
    }
}
