using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
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
                if (!uiObjectPtr.Valid)
                {
                    throw new Exception("UIComponent is not valid");
                }

                return new UIObject(uiObjectPtr);
            }
        }
    }
}
