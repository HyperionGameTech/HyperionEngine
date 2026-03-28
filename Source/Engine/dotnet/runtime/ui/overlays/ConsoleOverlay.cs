using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "ConsoleOverlay")]
    public class ConsoleOverlay : OverlayBase
    {
        public ConsoleOverlay()
        {
        }

        public override void Update(float delta)
        {
            InvokeNativeMethod(new Name("Update_Impl", weak: true), new object[] { delta });
        }

        public override UIObject CreateUIObject(UIObject spawnParent)
        {
            return InvokeNativeMethod<UIObject>(new Name("CreateUIObject_Impl", weak: true), new object[] { spawnParent });
        }

        public override bool IsEnabled()
        {
            return InvokeNativeMethod<bool>(new Name("IsEnabled_Impl", weak: true));
        }
    }
}