using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="LightComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 16)]
    public struct LightComponent : IComponent
    {
        [FieldOffset(0)]
        private Handle<Light> lightHandle;

        public void Dispose()
        {
            lightHandle.Dispose();
        }

        public Light? Light
        {
            get
            {
                return lightHandle.GetValue();
            }
            set
            {
                lightHandle.Dispose();

                if (value == null)
                {
                    lightHandle = Handle<Light>.Empty;
                    
                    return;
                }

                lightHandle = new Handle<Light>(value);
            }
        }
    }
}