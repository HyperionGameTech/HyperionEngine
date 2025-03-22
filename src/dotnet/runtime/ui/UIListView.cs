using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="UIListViewOrientation")]
    public enum UIListViewOrientation : byte
    {
        Vertical = 0,
        Horizontal
    }

    [HypClassBinding(Name="UIListView")]
    public class UIListView : UIObject
    {
        public UIListView() : base()
        {   
        }
    }
}