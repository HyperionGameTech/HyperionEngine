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
                return (Name)GetProperty(PropertyNames.Name)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }

        public Vec2i Position
        {
            get
            {
                return (Vec2i)GetProperty(PropertyNames.Position)
                    .InvokeGetter(this)
                    .GetValue();
            }
            set
            {
                GetProperty(PropertyNames.Position)
                    .InvokeSetter(this, new HypData(value));
            }
        }

        public Vec2i Size
        {
            get
            {
                return (Vec2i)GetProperty(PropertyNames.Size)
                    .InvokeGetter(this)
                    .GetValue();
            }
            set
            {
                GetProperty(PropertyNames.Size)
                    .InvokeSetter(this, new HypData(value));
            }
        }

        public UIObjectAlignment OriginAlignment
        {
            get
            {
                return (UIObjectAlignment)GetProperty(PropertyNames.OriginAlignment)
                    .InvokeGetter(this)
                    .GetValue();
            }
            set
            {
                GetProperty(PropertyNames.OriginAlignment)
                    .InvokeSetter(this, new HypData(value));
            }
        }

        public UIObjectAlignment ParentAlignment
        {
            get
            {
                return (UIObjectAlignment)GetProperty(PropertyNames.ParentAlignment)
                    .InvokeGetter(this)
                    .GetValue();
            }
            set
            {
                GetProperty(PropertyNames.ParentAlignment)
                    .InvokeSetter(this, new HypData(value));
            }
        }
    }
}