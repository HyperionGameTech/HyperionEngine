using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum UIObjectType : uint
    {
        Unknown = ~0u,
        Stage = 0,
        Button = 1,
        Text = 2,
        Panel = 3,
        Image = 4,
        TabView = 5,
        Tab = 6,
        Grid = 7,
        GridRow = 8,
        GridColumn = 9,
        MenuBar = 10,
        MenuItem = 11,
        DockableContainer = 12,
        DockableItem = 13,
        ListView = 14,
        ListViewItem = 15,
        Textbox = 16,
        Window = 17
    }

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