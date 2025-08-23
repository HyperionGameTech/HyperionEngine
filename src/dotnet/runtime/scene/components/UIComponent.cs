using System;
using System.Runtime.InteropServices;

namespace Hyperion
{   
    [HypClassBinding(Name="UIComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct UIComponent : IComponent
    {
        [FieldOffset(0)]
        private WeakHandle<UIObject> uiObject;

        public UIComponent()
        {
        }

        public void Dispose()
        {
        }

        public UIObject? UIObject
        {
            get
            {
                return uiObject.Lock().GetValue();
            }
        }
    }
}
