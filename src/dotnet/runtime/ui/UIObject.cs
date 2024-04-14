using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum UIObjectAlignment : uint
    {
        TopLeft = 0,
        TopRight = 1,
        Center = 2,
        BottomLeft = 3,
        BottomRight = 4
    }

    public class UIObject : IDisposable
    {
        private RefCountedPtr refCountedPtr = RefCountedPtr.Null;

        public UIObject()
        {
            this.refCountedPtr = RefCountedPtr.Null;
        }

        public UIObject(RefCountedPtr refCountedPtr)
        {
            this.refCountedPtr = refCountedPtr;

            if (this.refCountedPtr.Valid)
            {
                this.refCountedPtr.IncRef();
            }
        }

        public void Dispose()
        {
            if (this.refCountedPtr.Valid)
            {
                refCountedPtr.DecRef();
            }
        }

        public RefCountedPtr RefCountedPtr
        {
            get
            {
                return refCountedPtr;
            }
        }

        public Name Name
        {
            get
            {
                return UIObject_GetName(refCountedPtr);
            }
            set
            {
                UIObject_SetName(refCountedPtr, value);
            }
        }

        public Vec2i Position
        {
            get
            {
                Vec2i position = new Vec2i();
                UIObject_GetPosition(refCountedPtr, out position);
                return position;
            }
            set
            {
                UIObject_SetPosition(refCountedPtr, value);
            }
        }

        public Vec2i Size
        {
            get
            {
                Vec2i size = new Vec2i();
                UIObject_GetSize(refCountedPtr, out size);
                return size;
            }
            set
            {
                UIObject_SetSize(refCountedPtr, value);
            }
        }

        public UIObjectAlignment OriginAlignment
        {
            get
            {
                return UIObject_GetOriginAlignment(refCountedPtr);
            }
            set
            {
                UIObject_SetOriginAlignment(refCountedPtr, value);
            }
        }

        public UIObjectAlignment ParentAlignment
        {
            get
            {
                return UIObject_GetParentAlignment(refCountedPtr);
            }
            set
            {
                UIObject_SetParentAlignment(refCountedPtr, value);
            }
        }

        [DllImport("hyperion", EntryPoint = "UIObject_GetName")]
        private static extern Name UIObject_GetName(RefCountedPtr rc);

        [DllImport("hyperion", EntryPoint = "UIObject_SetName")]
        private static extern void UIObject_SetName(RefCountedPtr rc, Name name);

        [DllImport("hyperion", EntryPoint = "UIObject_GetPosition")]
        private static extern void UIObject_GetPosition(RefCountedPtr rc, [Out] out Vec2i position);

        [DllImport("hyperion", EntryPoint = "UIObject_SetPosition")]
        private static extern void UIObject_SetPosition(RefCountedPtr rc, Vec2i position);

        [DllImport("hyperion", EntryPoint = "UIObject_GetSize")]
        private static extern void UIObject_GetSize(RefCountedPtr rc, [Out] out Vec2i size);

        [DllImport("hyperion", EntryPoint = "UIObject_SetSize")]
        private static extern void UIObject_SetSize(RefCountedPtr rc, Vec2i size);

        [DllImport("hyperion", EntryPoint = "UIObject_GetOriginAlignment")]
        private static extern UIObjectAlignment UIObject_GetOriginAlignment(RefCountedPtr rc);

        [DllImport("hyperion", EntryPoint = "UIObject_SetOriginAlignment")]
        private static extern void UIObject_SetOriginAlignment(RefCountedPtr rc, UIObjectAlignment alignment);

        [DllImport("hyperion", EntryPoint = "UIObject_GetParentAlignment")]
        private static extern UIObjectAlignment UIObject_GetParentAlignment(RefCountedPtr rc);

        [DllImport("hyperion", EntryPoint = "UIObject_SetParentAlignment")]
        private static extern void UIObject_SetParentAlignment(RefCountedPtr rc, UIObjectAlignment alignment);
    }
}