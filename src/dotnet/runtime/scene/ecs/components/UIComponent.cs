using System;
using System.Runtime.InteropServices;

namespace Hyperion
{   
    [HypClassBinding(Name="UIComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct UIComponent : IComponent
    {
        [FieldOffset(0)]
        private IntPtr uiObjectPtr = IntPtr.Zero;

        public UIComponent()
        {
        }

        public UIObject UIObject
        {
            get
            {
                throw new NotImplementedException();
                // if (!uiObjectPtr.IsValid)
                // {
                //     throw new Exception("UIComponent is not valid");
                // }

                // return UIObjectHelpers.MarshalUIObject(uiObjectPtr);
            }
        }
    }
}
