using System;
using System.Runtime.InteropServices;

namespace Hyperion
{   
    [ClassBinding(Name="UIComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public ref struct UIComponent : IComponent
    {
        [FieldOffset(0)]
        private WeakHandle<UIObject> _uiObject;

        public UIComponent()
        {
        }

        public void Dispose()
        {
            _uiObject.Dispose();
        }

        public UIObject? UIObject => _uiObject.Lock().GetValue();
    }
}
