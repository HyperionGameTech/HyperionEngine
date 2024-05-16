using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class UIObjectHelpers
    {
        public static UIObject? MarshalUIObject(RefCountedPtr rc)
        {
            if (!rc.IsValid)
            {
                return null;
            }

            UIObjectType type = UIObject_GetType(rc);

            switch (type) {
            case UIObjectType.Unknown: // Custom override type
                return new UIObject(rc);
            case UIObjectType.Stage:
                return new UIStage(rc);
            case UIObjectType.Button:
                return new UIButton(rc);
            case UIObjectType.Text:
                return new UIText(rc);
            case UIObjectType.Panel:
                return new UIPanel(rc);
            case UIObjectType.Image:
                return new UIImage(rc);
            case UIObjectType.TabView:
                return new UITabView(rc);
            case UIObjectType.Tab:
                return new UITab(rc);
            case UIObjectType.Grid:
                return new UIGrid(rc);
            case UIObjectType.GridRow:
                return new UIGridRow(rc);
            case UIObjectType.GridColumn:
                return new UIGridColumn(rc);
            case UIObjectType.MenuBar:
                return new UIMenuBar(rc);
            case UIObjectType.MenuItem:
                return new UIMenuItem(rc);
            case UIObjectType.DockableItem:
                return new UIDockableItem(rc);
            case UIObjectType.DockableContainer:
                return new UIDockableContainer(rc);
            default:
                throw new Exception("Unknown UIObjectType!");
            }
        }

        [DllImport("hyperion", EntryPoint = "UIObject_GetType")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern UIObjectType UIObject_GetType(RefCountedPtr rc);
    }
}