using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "SaveProjectEditorTask")]
    public class SaveProjectEditorTask : TickableEditorTask
    {
        public SaveProjectEditorTask()
        {
        }
        
        public override void Tick(float delta)
        {
            InvokeNativeMethod(new Name("Tick_Impl", weak: true), new object[] { delta });
        }
    }
}