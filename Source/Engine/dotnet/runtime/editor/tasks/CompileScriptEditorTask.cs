using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(IsDynamic = true)]
    public class CompileScriptEditorTask : TickableEditorTask
    {
        private string ScriptPath { get; set; }

        public CompileScriptEditorTask(string scriptPath)
        {
            ScriptPath = scriptPath;
        }

        public void SetScriptPath(string path)
        {
            ScriptPath = path;
        }

        public override void Start()
        {
            // Set task properties before committing
            Title = $"Compiling {System.IO.Path.GetFileName(ScriptPath)}";
            Description = $"Compiling script...";
            
            // Call base Start which triggers Start_Impl on native side
            base.Start();
        }

        public override void Cancel()
        {
            InvokeNativeMethod(new Name("Cancel", weak: true));
        }

        public override bool IsCompleted()
        {
            return InvokeNativeMethod<bool>(new Name("IsCompleted", weak: true));
        }

        public override void Tick()
        {
            InvokeNativeMethod(new Name("Tick", weak: true));
        }
    }
}