using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="UIMenuBarDropDirection")]
    public enum UIMenuBarDropDirection : uint
    {
        Up,
        Down
    }

    [HypClassBinding(Name="UIMenuBar")]
    public class UIMenuBar : UIObject
    {
        public UIMenuBar() : base()
        {
                
        }
    }
}