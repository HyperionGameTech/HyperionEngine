using System.Net;
using System.Net.Sockets;
using Xunit;

namespace Hyperion.GameProjectTests;

public class PlayInEditorTests
{
    private const int NumFrames = 30;

    private static readonly TimeSpan ServerReadyTimeout = TimeSpan.FromMinutes(1);

    [Theory]
    [InlineData(GameProjectLanguage.Managed)]
    [InlineData(GameProjectLanguage.Native)]
    public void Standalone_SimulatesAndRestoresEditProject(GameProjectLanguage language)
    {
        GameProject project = GameProject.CreateInEditor("PlayStandalone" + language, language);

        GameAssert.Succeeded(project.Build());

        PlayRunResult result = EditorTestHost.Play(project, "standalone", "Standalone", NumFrames);

        AssertSimulated(project, result);

        Assert.False(result.Report!.ServerGame, "A standalone game was run as a server game");
        Assert.Equal("None", result.Report.NetStatus);
        Assert.True(result.Report.EditProjectRestored, "The edit project didn't come back after Stop");
    }

    [Theory]
    [InlineData(GameProjectLanguage.Managed)]
    [InlineData(GameProjectLanguage.Native)]
    public void DedicatedServer_HostsAndSimulatesAsServerGame(GameProjectLanguage language)
    {
        GameProject project = GameProject.CreateInEditor("PlayServer" + language, language);

        GameAssert.Succeeded(project.Build());

        PlayRunResult result = EditorTestHost.Play(project, "server", "DedicatedServer", NumFrames, port: FreeUdpPort());

        AssertSimulated(project, result);

        PlayReport report = result.Report!;

        Assert.True(report.ServerGame, "The game didn't run as a server game");
        Assert.Equal("Hosting", report.NetStatus);
        Assert.True(report.Hosting, "Nothing was listening on the play port while simulating");
        Assert.False(report.ListeningAfterStop, "The server was still listening after Stop");
        Assert.True(report.EditProjectRestored, "The edit project didn't come back after Stop");
    }

    [Theory]
    [InlineData(GameProjectLanguage.Managed)]
    [InlineData(GameProjectLanguage.Native)]
    public void Client_ConnectsToDedicatedServer_AndSimulates(GameProjectLanguage language)
    {
        GameProject project = GameProject.CreateInEditor("PlayClient" + language, language);

        GameAssert.Succeeded(project.Build());

        int port = FreeUdpPort();

        // the server holds on until the client has connected and disconnected again
        RunningPlay server = EditorTestHost.StartPlay(
            project,
            "server",
            "DedicatedServer",
            NumFrames,
            port: port,
            expectedClients: 1);

        PlayRunResult client;
        PlayRunResult serverRun;

        bool clientConnected = false;

        try
        {
            WaitForServerToListen(server);

            client = EditorTestHost.Play(project, "client", "Client", NumFrames, port: port);

            clientConnected = client.Report?.NetStatus == "Connected";
        }
        finally
        {
            // otherwise the server would wait out its timeout for a client that is never coming
            if (!clientConnected)
            {
                server.Process.Kill();
            }

            serverRun = server.Wait();
        }

        AssertSimulated(project, client);

        Assert.Equal("Connected", client.Report!.NetStatus);
        Assert.False(client.Report.ServerGame, "The client was run as a server game");
        Assert.True(client.Report.EditProjectRestored, "The client's edit project didn't come back after Stop");

        AssertSimulated(project, serverRun);

        PlayReport serverReport = serverRun.Report!;

        Assert.True(serverReport.ServerGame, "The server wasn't run as a server game");
        Assert.True(serverReport.ClientConnects >= 1, "The server never saw the client connect");
        Assert.True(serverReport.ClientDisconnects >= 1, "The server never saw the client disconnect after Stop");
        Assert.False(serverReport.ListeningAfterStop, "The server was still listening after Stop");
    }

    [Theory]
    [InlineData(GameProjectLanguage.Managed)]
    [InlineData(GameProjectLanguage.Native)]
    public void DedicatedServer_OnPortInUse_FailsAndLeavesEditProjectOpen(GameProjectLanguage language)
    {
        GameProject project = GameProject.CreateInEditor("PlayPortInUse" + language, language);

        GameAssert.Succeeded(project.Build());

        using UdpClient taken = new UdpClient(0);

        int port = ((IPEndPoint)taken.Client.LocalEndPoint!).Port;

        PlayRunResult result = EditorTestHost.Play(project, "taken", "DedicatedServer", NumFrames, port: port);

        Assert.True(result.Report != null, $"The editor wrote no results.\n{result.Process.Summary}");

        PlayReport report = result.Report!;

        Assert.True(report.Loaded, $"The project didn't open: {report.LoadError}\n{result.Process.Summary}");
        Assert.False(report.Started, "The server started on a port that was already in use");
        Assert.False(report.Hosting, "The server reported hosting on a port that was already in use");
        Assert.Equal("None", report.NetStatus);
        Assert.True(report.EditProjectRestored, "The edit project was closed when the server couldn't listen");
        Assert.Contains("could not listen on port", result.Process.Output);
        Assert.True(result.Process.Succeeded, $"The editor did not exit cleanly.\n{result.Process.Summary}");
    }

    private static void AssertSimulated(GameProject project, PlayRunResult result)
    {
        Assert.True(result.Report != null, $"The editor wrote no results.\n{result.Process.Summary}");

        PlayReport report = result.Report!;

        Assert.True(report.EditorLaunched, $"The editor game never launched.\n{result.Process.Summary}");
        Assert.True(report.Loaded, $"The project didn't open: {report.LoadError}\n{result.Process.Summary}");
        Assert.True(report.Started, $"The simulation didn't start.\n{result.Process.Summary}");
        Assert.Equal(project.ClassName, report.GameClass);
        Assert.True(report.Launched, $"{project.ClassName} never launched.\n{result.Process.Summary}");
        Assert.Contains($"{project.ClassName} launched", result.Process.Output);
        Assert.False(report.TimedOut, $"The simulation timed out.\n{result.Process.Summary}");
        Assert.True(report.SimulatedFrames >= NumFrames, $"Simulated {report.SimulatedFrames} frames, expected at least {NumFrames}");
        Assert.True(report.GameTime > 0.0, "Game time didn't advance while simulating");
        Assert.True(report.Stopped, "The simulation didn't stop");
        Assert.True(result.Process.Succeeded, $"The game simulated, but the editor did not exit cleanly.\n{result.Process.Summary}");
    }

    private static void WaitForServerToListen(RunningPlay server)
    {
        DateTime deadline = DateTime.UtcNow + ServerReadyTimeout;

        while (!server.Process.OutputSoFar.Contains("listening on port"))
        {
            if (server.Process.HasExited || DateTime.UtcNow > deadline)
            {
                server.Process.Kill();

                PlayRunResult failed = server.Wait();

                Assert.Fail($"The dedicated server never started listening.\n{failed.Process.Summary}");
            }

            Thread.Sleep(100);
        }
    }

    private static int FreeUdpPort()
    {
        using UdpClient probe = new UdpClient(0);

        return ((IPEndPoint)probe.Client.LocalEndPoint!).Port;
    }
}
