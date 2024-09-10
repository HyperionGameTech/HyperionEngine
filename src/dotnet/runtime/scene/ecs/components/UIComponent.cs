using System;
using System.Runtime.InteropServices;

namespace Hyperion
{   
    [HypClassBinding(Name="UIComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct UIComponent : IComponent
    {
        [FieldOffset(0)]
        private RefCountedPtr uiObjectPtr = RefCountedPtr.Null;

        public UIComponent()
        {
        }

        public UIObject UIObject
        {
            get
            {
                if (!uiObjectPtr.IsValid)
                {
                    throw new Exception("UIComponent is not valid");
                }

                return UIObjectHelpers.MarshalUIObject(uiObjectPtr);
            }
        }
    }
}
