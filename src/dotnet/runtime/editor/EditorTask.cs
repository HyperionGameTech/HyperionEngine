using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="EditorTaskBase")]
    public abstract class EditorTaskBase : HypObject
    {
    }

    [ClassBinding(Name="TickableEditorTask")]
    public abstract class TickableEditorTask : EditorTaskBase
    {
        public TickableEditorTask()
        {
        }


        public abstract void Cancel();
        public abstract bool IsCompleted();
        public abstract void Process();
    }

    [ClassBinding(Name="LongRunningEditorTask")]
    public abstract class LongRunningEditorTask : EditorTaskBase
    {
        public LongRunningEditorTask()
        {
        }


        public abstract void Cancel();
        public abstract bool IsCompleted();
        public abstract void Process();
    }
}