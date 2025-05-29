using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name = "GenerateLightmapsEditorTask")]
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

        public override void Process()
        {
            InvokeNativeMethod(new Name("Process", weak: true));
        }
    }
}