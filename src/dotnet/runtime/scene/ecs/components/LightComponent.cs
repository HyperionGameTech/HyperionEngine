using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="LightComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
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

                //@FIXME Dispose would need to get called on all components to ensure proper refcounts.
                // We should just decrement the ref count that was added by Handle<T> (C#) and just use the managed ref count (by HypObject_OnIncRefCount)
                lightHandle = new Handle<Light>(value);
            }
        }
    }
}