using System;
using System.Runtime.InteropServices;

namespace Hyperion
{   
    [HypClassBinding(Name="UIComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct UIComponent : IComponent
    {
        [FieldOffset(0)]
        private Ptr<UIObject> uiObject;

        public UIComponent()
        {
        }

        public UIObject UIObject
        {
            get
            {
                return (UIObject)uiObject.GetValue();
            }
        }
    }
}
