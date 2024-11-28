using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
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