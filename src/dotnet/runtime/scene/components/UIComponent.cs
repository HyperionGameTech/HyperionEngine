using System;
using System.Runtime.InteropServices;

namespace Hyperion
{   
    [ClassBinding(Name="UIComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public ref struct UIComponent : IComponent
    {
        public static Class Class => Class.GetClass(typeof(UIComponent));

        [FieldOffset(0)]
        private WeakHandle<UIObject> _uiObject;

        public UIComponent()
        {
        }

        public void Dispose()
        {
        }

        public UIObject? UIObject => _uiObject.Lock().GetValue();

        public unsafe IntPtr NativeAddress
        {
            get
            {
                fixed (UIComponent* pThis = &this)
                {
                    return (IntPtr)pThis;
                }
            }
        }
    }
}
