using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorActionBase")]
    public abstract class EditorActionBase : ObjectBase
    {
        public EditorActionBase()
        {
        }

        public abstract string GetText();
        public abstract void Execute(EditorSubsystem editorSubsystem, EditorProject editorProject);
        public abstract void Revert(EditorSubsystem editorSubsystem, EditorProject editorProject);
    }

    /// <summary>
    /// C++ only - Use EditorAction instead
    /// </summary>
    [ClassBinding(Name = "FunctionalEditorAction")]
    public class FunctionalEditorAction : EditorActionBase
    {
        public override string GetText()
        {
            return InvokeNativeMethod<string>(new Name("GetText", weak: true));
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

    [ClassBinding(IsDynamic = true)]
    public class EditorAction : EditorActionBase
    {
        private string _text;
        private Action<EditorSubsystem, EditorProject> _execute;
        private Action<EditorSubsystem, EditorProject> _revert;

        public EditorAction(string text, Action<EditorSubsystem, EditorProject> execute, Action<EditorSubsystem, EditorProject> revert) : base()
        {
            _text = text;
            _execute = execute;
            _revert = revert;
        }

        public override string GetText()
        {
            return _text;
        }

        public override void Execute(EditorSubsystem editorSubsystem, EditorProject editorProject)
        {
            _execute(editorSubsystem, editorProject);
        }

        public override void Revert(EditorSubsystem editorSubsystem, EditorProject editorProject)
        {
            _revert(editorSubsystem, editorProject);
        }
    }
}