using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "GenerateBentNormalsEditorTask")]
    public class GenerateBentNormalsEditorTask : TickableEditorTask
    {
        public GenerateBentNormalsEditorTask()
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

        public override void Tick()
        {
            InvokeNativeMethod(new Name("Tick", weak: true));
        }
    }
}