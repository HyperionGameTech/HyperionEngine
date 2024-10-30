using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum UIObjectType : uint
    {
        Unknown = ~0u,
        Object = 0,
        Stage = 1,
        Button = 2,
        Text = 3,
        Panel = 4,
        Image = 5,
        TabView = 6,
        Tab = 7,
        Grid = 8,
        GridRow = 9,
        GridColumn = 10,
        MenuBar = 11,
        MenuItem = 12,
        DockableContainer = 13,
        DockableItem = 14,
        ListView = 15,
        ListViewItem = 16,
        Textbox = 17,
        Window = 18
    }

    [Flags]
    public enum UIObjectScrollbarOrientation : byte
    {
        None = 0x0,
        Horizontal = 0x1,
        Vertical = 0x2
    }

    [StructLayout(LayoutKind.Explicit, Size=16, Pack=8)]
    public struct UIEventHandlerResult
    {
        public static readonly UIEventHandlerResult Error = new UIEventHandlerResult(0x1u << 31);
        public static readonly UIEventHandlerResult Ok = new UIEventHandlerResult(0x0);
        public static readonly UIEventHandlerResult StopBubbling = new UIEventHandlerResult(0x1);

        [FieldOffset(0)]
        private uint code;

        [FieldOffset(8)]
        private IntPtr message;

        public UIEventHandlerResult(uint code)
        {
            this.code = code;
            this.message = IntPtr.Zero;
        }

        public uint Code
        {
            get
            {
                return code;
            }
        }

        public string Message
        {
            get
            {
                return Marshal.PtrToStringAnsi(message);
            }
        }

        public static bool operator ==(UIEventHandlerResult a, UIEventHandlerResult b)
        {
            return a.code == b.code;
        }

        public static bool operator !=(UIEventHandlerResult a, UIEventHandlerResult b)
        {
            return a.code != b.code;
        }

        public static UIEventHandlerResult operator |(UIEventHandlerResult a, UIEventHandlerResult b)
        {
            return new UIEventHandlerResult(a.code | b.code);
        }

        public static UIEventHandlerResult operator &(UIEventHandlerResult a, UIEventHandlerResult b)
        {
            return new UIEventHandlerResult(a.code & b.code);
        }

        public static UIEventHandlerResult operator ~(UIEventHandlerResult a)
        {
            return new UIEventHandlerResult(~a.code);
        }

        public override bool Equals(object obj)
        {
            if (obj is UIEventHandlerResult)
            {
                return this == (UIEventHandlerResult)obj;
            }

            return false;
        }
    }

    public enum UIObjectAlignment : uint
    {
        TopLeft = 0,
        TopRight = 1,
        Center = 2,
        BottomLeft = 3,
        BottomRight = 4
    }

    [Flags]
    public enum UIObjectSizeFlags : uint
    {
        Auto = 0x04,
        Pixel = 0x10,
        Percent = 0x20,
        Fill = 0x40,
        Default = Pixel
    }

    [HypClassBinding(Name="UIObjectSize")]
    [StructLayout(LayoutKind.Sequential, Size=16)]
    public unsafe struct UIObjectSize
    {
        public static readonly UIObjectSizeFlags Auto = UIObjectSizeFlags.Auto;
        public static readonly UIObjectSizeFlags Pixel = UIObjectSizeFlags.Pixel;
        public static readonly UIObjectSizeFlags Percent = UIObjectSizeFlags.Percent;
        public static readonly UIObjectSizeFlags Fill = UIObjectSizeFlags.Fill;
        public static readonly UIObjectSizeFlags Default = UIObjectSizeFlags.Default;

        private unsafe fixed uint flags[2];
        private Vec2i value;

        public UIObjectSize()
        {
            this.flags[0] = (uint)UIObjectSizeFlags.Default;
            this.flags[1] = (uint)UIObjectSizeFlags.Default;
            this.value = new Vec2i();
        }

        public UIObjectSize(Vec2i value)
        {
            this.flags[0] = (uint)UIObjectSizeFlags.Default;
            this.flags[1] = (uint)UIObjectSizeFlags.Default;
            this.value = value;
        }

        public UIObjectSize(Vec2i value, UIObjectSizeFlags flags)
        {
            this.flags[0] = (uint)flags;
            this.flags[1] = (uint)flags;
            this.value = value;

            ApplyDefaultFlags();
        }

        public UIObjectSize(UIObjectSizeFlags flags)
        {
            this.flags[0] = (uint)flags;
            this.flags[1] = (uint)flags;
            this.value = new Vec2i();

            ApplyDefaultFlags();
        }

        public UIObjectSize(int x, UIObjectSizeFlags flagsX, int y, UIObjectSizeFlags flagsY)
        {
            this.flags[0] = (uint)flagsX;
            this.flags[1] = (uint)flagsY;
            this.value = new Vec2i(x, y);

            ApplyDefaultFlags();
        }

        private void ApplyDefaultFlagMask(uint mask)
        {
            for (int i = 0; i < 2; i++)
            {
                if ((flags[i] & mask) == 0)
                {
                    flags[i] |= (((uint)UIObjectSizeFlags.Default) & mask);
                }
            }
        }

        private void ApplyDefaultFlags()
        {
            ApplyDefaultFlagMask((uint)UIObjectSizeFlags.Pixel | (uint)UIObjectSizeFlags.Percent | (uint)UIObjectSizeFlags.Fill);
        }
    }

    [HypClassBinding(Name="UIObject")]
    public class UIObject : HypObject
    {
        public UIObject()
        {
        }
    }
}