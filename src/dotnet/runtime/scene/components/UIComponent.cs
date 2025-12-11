using System;
using System.Runtime.InteropServices;

namespace Hyperion
{   
    [ClassBinding(Name="UIComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public ref struct UIComponent : IComponent
    {
        [FieldOffset(0)]
        private WeakHandle<UIObject> uiObject;

        public UIComponent()
        {
        }

        public void Dispose()
        {
        }

        public UIObject? UIObject => uiObject.Lock().GetValue();

        public static Class Class => Class.GetClass(typeof(UIComponent));
    }
}
