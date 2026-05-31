using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="UIListViewOrientation")]
    public enum UIListViewOrientation : byte
    {
        Vertical = 0,
        Horizontal
    }

    [ClassBinding(Name="UIListView")]
    public class UIListView : UIObject
    {
        public UIListView() : base()
        {   
        }
    }
}