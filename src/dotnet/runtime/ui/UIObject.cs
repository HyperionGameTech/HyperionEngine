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

    [HypClassBinding(Name="UIObject")]
    public class UIObject : HypObject
    {
        public UIObject()
        {
        }

        public Name Name
        {
            get
            {
                return (Name)GetMethod(new Name("GetName", weak: true))
                    .Invoke(this)
                    .GetValue();
            }
            set
            {
                GetMethod(new Name("SetName", weak: true))
                    .Invoke(this, value);
            }
        }

        public Vec2i Position
        {
            get
            {
                return (Vec2i)GetMethod(new Name("GetPosition", weak: true))
                    .Invoke(this)
                    .GetValue();
            }
            set
            {
                GetMethod(new Name("SetPosition", weak: true))
                    .Invoke(this, value);
            }
        }

        public Vec2i ActualSize
        {
            get
            {
                return (Vec2i)GetMethod(new Name("GetActualSize", weak: true))
                    .Invoke(this)
                    .GetValue();
            }
        }

        public UIObjectAlignment OriginAlignment
        {
            get
            {
                return (UIObjectAlignment)GetMethod(new Name("GetOriginAlignment", weak: true))
                    .Invoke(this)
                    .GetValue();
            }
            set
            {
                GetMethod(new Name("SetOriginAlignment", weak: true))
                    .Invoke(this, value);
            }
        }

        public UIObjectAlignment ParentAlignment
        {
            get
            {
                return (UIObjectAlignment)GetMethod(new Name("GetParentAlignment", weak: true))
                    .Invoke(this)
                    .GetValue();
            }
            set
            {
                GetMethod(new Name("SetParentAlignment", weak: true))
                    .Invoke(this, value);
            }
        }
    }
}