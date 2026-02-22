using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="UIMenuBarDropDirection")]
    public enum UIMenuBarDropDirection : uint
    {
        Up,
        Down
    }

    [ClassBinding(Name="UIMenuBar")]
    public class UIMenuBar : UIObject
    {
        public UIMenuBar() : base()
        {
                
        }
    }
}