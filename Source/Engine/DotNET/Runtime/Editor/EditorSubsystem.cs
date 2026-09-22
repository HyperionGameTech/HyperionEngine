using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "MeshEditFaceMode")]
    public enum MeshEditFaceMode : byte
    {
        Triangle = 0,
        Quad
    }

    [ClassBinding(Name = "EditorPlayNetMode")]
    public enum EditorPlayNetMode : byte
    {
        Standalone = 0,
        Client,
        DedicatedServer
    }

    [ClassBinding(Name = "EditorPlayNetStatus")]
    public enum EditorPlayNetStatus : byte
    {
        None = 0,
        Connecting,
        Connected,
        Failed,
        Disconnected,
        Hosting
    }

    [ClassBinding(Name = "EditorSubsystem")]
    public class EditorSubsystem : Subsystem
    {
        public EditorSubsystem()
        {
        }

        public EditorProject? CurrentProject => this.GetCurrentProject(); // extension method
        public EditorGizmoBase? SelectedGizmo => this.GetSelectedGizmo(); // extension method
        public EditorViewport? ActiveViewport => this.GetActiveViewport(); // extension method

        public Scene? ActiveScene
        {
            get => this.GetActiveScene();
            set => this.SetActiveScene(value);
        }

        public EditorTerrainState? EditorTerrainState => InvokeNativeMethod<EditorTerrainState>(new Name("GetTerrainState"));

        public EditorDecalPainterState? EditorDecalPainterState => InvokeNativeMethod<EditorDecalPainterState>(new Name("GetDecalPainterState"));

        public void ExecuteCommandByName(Name commandName, params string[] arguments)
        {
            this.InvokeNativeMethod(new Name("ExecuteCommandByName"), new object[] { commandName, string.Join(" ", arguments) });
        }

        public string GetCodeEditor()
        {
            return this.InvokeNativeMethod<string>(new Name("GetCodeEditor")) ?? "VSCode";
        }
    }
}