using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum UIEventHandlerResult : uint
    {
        Error = 0x1u << 31,
        Ok = 0x0,
        StopBubbling = 0x1
    }

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
                Name name = new Name(0);
                UIObject_GetName(refCountedPtr, out name);
                return name;
            }
            set
            {
                UIObject_SetName(refCountedPtr, ref value);
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
                UIObject_SetPosition(refCountedPtr, ref value);
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
                UIObject_SetSize(refCountedPtr, ref value);
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
        private static extern void UIObject_GetName(RefCountedPtr rc, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "UIObject_SetName")]
        private static extern void UIObject_SetName(RefCountedPtr rc, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "UIObject_GetPosition")]
        private static extern void UIObject_GetPosition(RefCountedPtr rc, [Out] out Vec2i position);

        [DllImport("hyperion", EntryPoint = "UIObject_SetPosition")]
        private static extern void UIObject_SetPosition(RefCountedPtr rc, [In] ref Vec2i position);

        [DllImport("hyperion", EntryPoint = "UIObject_GetSize")]
        private static extern void UIObject_GetSize(RefCountedPtr rc, [Out] out Vec2i size);

        [DllImport("hyperion", EntryPoint = "UIObject_SetSize")]
        private static extern void UIObject_SetSize(RefCountedPtr rc, [In] ref Vec2i size);

        [DllImport("hyperion", EntryPoint = "UIObject_GetOriginAlignment")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern UIObjectAlignment UIObject_GetOriginAlignment(RefCountedPtr rc);

        [DllImport("hyperion", EntryPoint = "UIObject_SetOriginAlignment")]
        private static extern void UIObject_SetOriginAlignment(RefCountedPtr rc, [MarshalAs(UnmanagedType.U4)] UIObjectAlignment alignment);

        [DllImport("hyperion", EntryPoint = "UIObject_GetParentAlignment")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern UIObjectAlignment UIObject_GetParentAlignment(RefCountedPtr rc);

        [DllImport("hyperion", EntryPoint = "UIObject_SetParentAlignment")]
        private static extern void UIObject_SetParentAlignment(RefCountedPtr rc, [MarshalAs(UnmanagedType.U4)] UIObjectAlignment alignment);
    }
}