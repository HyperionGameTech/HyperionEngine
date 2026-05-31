using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(IsDynamic = true)]
    public class CompileScriptEditorTask : TickableEditorTask
    {
        private string ScriptPath { get; set; }
        private bool _isCompleted = false;

        public Action? OnCancelAction { get; set; }

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
            if (OnCancelAction != null)
            {
                OnCancelAction();
            }
        }

        public override bool IsCompleted()
        {
            return _isCompleted;
        }

        public override void Tick()
        {
            // do nothing
        }

        public void SetIsCompleted(bool isCompleted)
        {
            _isCompleted = isCompleted;
        }
    }
}