using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorTaskBase")]
    public abstract class EditorTaskBase : ObjectBase
    {
        public float Progress => this.GetProgress();

        public abstract void Cancel();
        public abstract bool IsCompleted();

        public virtual void Start()
        {
            // call native as default impl
            this.InvokeNativeMethod(new Name("Start_Impl", weak: true));
        }
    }

    [ClassBinding(Name = "TickableEditorTask")]
    public abstract class TickableEditorTask : EditorTaskBase
    {
        public TickableEditorTask()
        {
        }

        public bool IsForegroundTask => this.IsForegroundTask();

        public override void Cancel()
        {
            // call native as default impl
            InvokeNativeMethod(new Name("Cancel_Impl", weak: true));
        }

        public override bool IsCompleted()
        {
            // call native as default impl
            return InvokeNativeMethod<bool>(new Name("IsCompleted_Impl", weak: true));
        }

        public abstract void Tick(float delta);
    }

    [ClassBinding(Name = "LongRunningEditorTask")]
    public abstract class LongRunningEditorTask : EditorTaskBase
    {
        public LongRunningEditorTask()
        {
        }

        public override void Cancel()
        {
            // call native as default impl
            InvokeNativeMethod(new Name("Cancel_Impl", weak: true));
        }

        public override bool IsCompleted()
        {
            // call native as default impl
            return InvokeNativeMethod<bool>(new Name("IsCompleted_Impl", weak: true));
        }

        public abstract void Process();
    }
}