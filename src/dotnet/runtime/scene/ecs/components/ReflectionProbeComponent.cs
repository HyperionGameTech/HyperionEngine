using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="ReflectionProbeComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 32)]
    public struct ReflectionProbeComponent : IComponent
    {
        [FieldOffset(0)]
        private Vec2u dimensions = new Vec2u(256, 256);

        [FieldOffset(8)]
        private Handle<EnvProbe> envProbeHandle;

        [FieldOffset(16)]
        private RefCountedPtr renderSubsystem;

        public ReflectionProbeComponent()
        {
        }

        public void Dispose()
        {
            envProbeHandle.Dispose();
        }

        public Vec2u Dimensions
        {
            get
            {
                return dimensions;
            }
            set
            {
                dimensions = value;
            }
        }

        public EnvProbe EnvProbe
        {
            get
            {
                return envProbeHandle.GetValue();
            }
            set
            {
                envProbeHandle.Dispose();

                if (value == null)
                {
                    envProbeHandle = Handle<EnvProbe>.Empty;
                    
                    return;
                }

                envProbeHandle = new Handle<EnvProbe>(value);
            }
        }
    }
}