using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "OverlayBase")]
    public abstract class OverlayBase : ObjectBase
    {
        public OverlayBase()
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

    [ClassBinding(Name = "TextureOverlay")]
    public class TextureOverlay : OverlayBase
    {
        public TextureOverlay()
        {
        }

        public override void Update(float delta)
        {
            InvokeNativeMethod(new Name("Update_Impl", weak: true), [delta]);
        }

        public override UIObject CreateUIObject(UIObject spawnParent)
        {
            return InvokeNativeMethod<UIObject>(new Name("CreateUIObject_Impl", weak: true), [spawnParent]);
        }

        public override bool IsEnabled()
        {
            return InvokeNativeMethod<bool>(new Name("IsEnabled_Impl", weak: true));
        }
    }

    [ClassBinding(Name = "TextOverlay")]
    public class TextOverlay : OverlayBase
    {
        public TextOverlay()
        {
        }

        public override void Update(float delta)
        {
            InvokeNativeMethod(new Name("Update_Impl", weak: true), [delta]);
        }

        public override UIObject CreateUIObject(UIObject spawnParent)
        {
            return InvokeNativeMethod<UIObject>(new Name("CreateUIObject_Impl", weak: true), [spawnParent]);
        }

        public override bool IsEnabled()
        {
            return InvokeNativeMethod<bool>(new Name("IsEnabled_Impl", weak: true));
        }
    }
}