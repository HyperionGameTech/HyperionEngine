using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "GenerateLightmapsEditorTask")]
    public class GenerateLightmapsEditorTask : TickableEditorTask
    {
        public GenerateLightmapsEditorTask()
        {
        }

        public override void Cancel()
        {
            InvokeNativeMethod(new Name("Cancel", weak: true));
        }

        public override bool IsCompleted()
        {
            return InvokeNativeMethod<bool>(new Name("IsCompleted", weak: true));
        }

        public override void Start()
        {
            InvokeNativeMethod(new Name("Start", weak: true));
        }

        public override void Tick(float delta)
        {
            InvokeNativeMethod(new Name("Tick", weak: true), new object[] { delta });
        }
    }
}