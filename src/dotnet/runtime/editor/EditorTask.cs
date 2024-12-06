using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="IEditorTask")]
    public abstract class IEditorTask : HypObject
    {
    }

    [HypClassBinding(Name="TickableEditorTask")]
    public abstract class TickableEditorTask : IEditorTask
    {
        public TickableEditorTask()
        {
        }


        public abstract void Cancel();
        public abstract bool IsCompleted();
        public abstract void Process();
    }

    [HypClassBinding(Name="LongRunningEditorTask")]
    public abstract class LongRunningEditorTask : IEditorTask
    {
        public LongRunningEditorTask()
        {
        }


        public abstract void Cancel();
        public abstract bool IsCompleted();
        public abstract void Process();
    }
}