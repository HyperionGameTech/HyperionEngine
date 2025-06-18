using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="EditorActionStackState")]
    [Flags]
    public enum EditorActionStackState : uint
    {
        None = 0,
        CanUndo = 0x1,
        CanRedo = 0x2
    }

    [HypClassBinding(Name="EditorActionBase")]
    public abstract class EditorActionBase : HypObject
    {
        public EditorActionBase()
        {
        }

        public abstract Name GetName();
        public abstract void Execute(EditorSubsystem editorSubsystem, EditorProject editorProject);
        public abstract void Revert(EditorSubsystem editorSubsystem, EditorProject editorProject);
    }

    /// <summary>
    /// C++ only - Use EditorAction instead
    /// </summary>
    [HypClassBinding(Name="FunctionalEditorAction")]
    public class FunctionalEditorAction : EditorActionBase
    {
        public override Name GetName()
        {
            return InvokeNativeMethod<Name>(new Name("GetName", weak: true));
        }

        public override void Execute(EditorSubsystem editorSubsystem, EditorProject editorProject)
        {
            InvokeNativeMethod(new Name("Execute", weak: true), new object[] { editorSubsystem, editorProject });
        }

        public override void Revert(EditorSubsystem editorSubsystem, EditorProject editorProject)
        {
            InvokeNativeMethod(new Name("Revert", weak: true), new object[] { editorSubsystem, editorProject });
        }
    }

    public class EditorAction : EditorActionBase
    {
        private Name name;
        private Action<EditorSubsystem, EditorProject> execute;
        private Action<EditorSubsystem, EditorProject> revert;

        public EditorAction(Name name, Action<EditorSubsystem, EditorProject> execute, Action<EditorSubsystem, EditorProject> revert) : base()
        {
            this.name = name;
            this.execute = execute;
            this.revert = revert;
        }

        public override Name GetName()
        {
            return name;
        }

        public override void Execute(EditorSubsystem editorSubsystem, EditorProject editorProject)
        {
            Logger.Log(LogType.Info, "Execute action: " + name);

            execute(editorSubsystem, editorProject);
        }

        public override void Revert(EditorSubsystem editorSubsystem, EditorProject editorProject)
        {
            Logger.Log(LogType.Info, "Revert action: " + name);

            revert(editorSubsystem, editorProject);
        }
    }
}