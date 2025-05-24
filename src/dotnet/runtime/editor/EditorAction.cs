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

    [HypClassBinding(Name="IEditorAction")]
    public abstract class IEditorAction : HypObject
    {
        public IEditorAction()
        {
        }

        public abstract Name GetName();
        public abstract void Execute();
        public abstract void Revert();
    }

    /// <summary>
    /// C++ only - Use EditorAction instead
    /// </summary>
    [HypClassBinding(Name="FunctionalEditorAction")]
    public class FunctionalEditorAction : IEditorAction
    {
        public override Name GetName()
        {
            return InvokeNativeMethod<Name>(new Name("GetName", weak: true));
        }

        public override void Execute()
        {
            InvokeNativeMethod(new Name("Execute", weak: true));
        }

        public override void Revert()
        {
            InvokeNativeMethod(new Name("Revert", weak: true));
        }
    }

    public class EditorAction : IEditorAction
    {
        private Name name;
        private Action execute;
        private Action revert;

        public EditorAction(Name name, Action execute, Action revert) : base()
        {
            this.name = name;
            this.execute = execute;
            this.revert = revert;
        }

        public override Name GetName()
        {
            return name;
        }

        public override void Execute()
        {
            Logger.Log(LogType.Info, "Execute action: " + name);

            execute();
        }

        public override void Revert()
        {
            Logger.Log(LogType.Info, "Revert action: " + name);

            revert();
        }
    }
}