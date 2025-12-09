using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorDebugOverlayBase")]
    public abstract class EditorDebugOverlayBase : ObjectBase
    {
        public EditorDebugOverlayBase()
        {
        }

        // 0 = top-left, 1 = bottom-left, 2 = top-right, 3 = bottom-right
        public virtual int GetPlacement()
        {
            return InvokeNativeMethod<int>(new Name("GetPlacement_Impl", weak: true));
        }

        public abstract void Update(float delta);

        public abstract UIObject CreateUIObject(UIObject spawnParent);
        public abstract bool IsEnabled();
    }
}