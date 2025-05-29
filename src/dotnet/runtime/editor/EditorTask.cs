using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="EditorTaskBase")]
    public abstract class EditorTaskBase : HypObject
    {
    }

    [HypClassBinding(Name="TickableEditorTask")]
    public abstract class TickableEditorTask : EditorTaskBase
    {
        public TickableEditorTask()
        {
        }


        public abstract void Cancel();
        public abstract bool IsCompleted();
        public abstract void Process();
    }

    [HypClassBinding(Name="LongRunningEditorTask")]
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