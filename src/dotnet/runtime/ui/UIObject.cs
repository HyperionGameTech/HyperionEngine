using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="UIObjectType")]
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

    [HypClassBinding(Name="UIObjectScrollbarOrientation")]
    [Flags]
    public enum UIObjectScrollbarOrientation : byte
    {
        None = 0x0,
        Horizontal = 0x1,
        Vertical = 0x2
    }
    [HypClassBinding(Name="UIEventHandlerResult")]
    [StructLayout(LayoutKind.Explicit, Size=24, Pack=8)]
    public struct UIEventHandlerResult
    {
        public static readonly UIEventHandlerResult Error = new UIEventHandlerResult(0x1u << 31);
        public static readonly UIEventHandlerResult Ok = new UIEventHandlerResult(0x0);
        public static readonly UIEventHandlerResult StopBubbling = new UIEventHandlerResult(0x1);

        [FieldOffset(0)]
        private uint code;

        [FieldOffset(8)]
        private IntPtr messagePtr;

        [FieldOffset(8)]
        private IntPtr functionNamePtr;

        public UIEventHandlerResult(uint code)
        {
            this.code = code;
            this.messagePtr = IntPtr.Zero;
            this.functionNamePtr = IntPtr.Zero;
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
                if (messagePtr == IntPtr.Zero)
                {
                    return string.Empty;
                }
                
                IntPtr stringPtr = UIEventHandlerResult_GetMessage(ref this);

                if (stringPtr == IntPtr.Zero)
                {
                    return string.Empty;
                }

                return Marshal.PtrToStringUTF8(stringPtr);
            }
        }

        public string FunctionName
        {
            get
            {
                if (functionNamePtr == IntPtr.Zero)
                {
                    return string.Empty;
                }
                
                IntPtr stringPtr = UIEventHandlerResult_GetFunctionName(ref this);

                if (stringPtr == IntPtr.Zero)
                {
                    return string.Empty;
                }

                return Marshal.PtrToStringAnsi(stringPtr);
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

        [DllImport("hyperion", EntryPoint="UIEventHandlerResult_GetMessage")]
        private static extern IntPtr UIEventHandlerResult_GetMessage(ref UIEventHandlerResult result);

        [DllImport("hyperion", EntryPoint="UIEventHandlerResult_GetFunctionName")]
        private static extern IntPtr UIEventHandlerResult_GetFunctionName(ref UIEventHandlerResult result);
    }

    [HypClassBinding(Name="UIObjectPositioning")]
    public enum UIObjectPositioning : uint
    {
        Default = 0,
        Absolute = 1
    }

    [HypClassBinding(Name="UIObjectAlignment")]
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

    [HypClassBinding(Name="UIObjectBorderFlags")]
    [Flags]
    public enum UIObjectBorderFlags : uint
    {
        None = 0x0,
        Top = 0x1,
        Left = 0x2,
        Bottom = 0x4,
        Right = 0x8,
        All = Top | Left | Bottom | Right
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

        private void ApplyDefaultFlags()
        {
            for (int i = 0; i < 2; i++)
            {
                if (flags[i] == 0)
                {
                    flags[i] |= (uint)UIObjectSizeFlags.Default;
                }
            }
        }

        public override string ToString()
        {
            return $"UIObjectSize(x: ({value.X}, {flags[0]}), y: ({value.Y}, {flags[1]})";
        }
    }

    [HypClassBinding(Name="UIObject")]
    public class UIObject : HypObject
    {
        public UIObject()
        {
        }

        public T Spawn<T>() where T : UIObject
        {
            return Spawn<T>(Name.Invalid);
        }

        public T Spawn<T>(Name name) where T : UIObject
        {
            return Spawn<T>(name, new Vec2i(0, 0));
        }

        public T Spawn<T>(Name name, Vec2i position) where T : UIObject
        {
            return Spawn<T>(name, position, new UIObjectSize(UIObjectSizeFlags.Auto));
        }

        public T Spawn<T>(Name name, Vec2i position, UIObjectSize size) where T : UIObject
        {
            HypData hypData = new HypData(null);

            UIObject_Spawn(NativeAddress, HypClass.GetClass(typeof(T)).Address, ref name, ref position, ref size, out hypData.Buffer);

            return (T)hypData.GetValue();
        }

        public UIObject? Find(Name name)
        {
            HypDataBuffer hypDataBuffer;

            if (!UIObject_Find(NativeAddress, HypClass.GetClass(typeof(UIObject)).Address, ref name, out hypDataBuffer))
            {
                return null;
            }

            UIObject? result = (UIObject?)hypDataBuffer.GetValue();

            hypDataBuffer.Destruct();

            return result;
        }

        public T? Find<T>(Name name) where T : UIObject
        {
            HypDataBuffer hypDataBuffer;

            if (!UIObject_Find(NativeAddress, HypClass.GetClass(typeof(T)).Address, ref name, out hypDataBuffer))
            {
                return null;
            }

            T? result = (T?)hypDataBuffer.GetValue();

            hypDataBuffer.Destruct();

            return result;
        }

        [DllImport("hyperion", EntryPoint="UIObject_Spawn")]
        private static extern void UIObject_Spawn([In] IntPtr spawnParentPtr, [In] IntPtr hypClassPtr, [In] ref Name name, [In] ref Vec2i position, [In] ref UIObjectSize size, [Out] out HypDataBuffer outHypData);

        [DllImport("hyperion", EntryPoint="UIObject_Find")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool UIObject_Find([In] IntPtr parentPtr, [In] IntPtr hypClassPtr, [In] ref Name name, [Out] out HypDataBuffer outHypData);
    }
}