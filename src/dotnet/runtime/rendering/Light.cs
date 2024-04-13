using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum LightType : uint
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    }

    public class Light : IDisposable
    {
        private ManagedHandle handle;

        public Light(LightType type, Vec3f position, Color color, float intensity, float radius)
        {
            handle = Light_Create(
                type,
                position,
                color,
                intensity,
                radius
            );
        }

        public Light(ManagedHandle handle)
        {
            this.handle = handle;
            this.handle.IncRef(Light_GetTypeID());
        }

        public void Dispose()
        {
            handle.DecRef(Light_GetTypeID());
        }

        public void Init()
        {
            Light_Init(handle);
        }

        public ManagedHandle Handle
        {
            get
            {
                return handle;
            }
        }

        public uint ID
        {
            get
            {
                return handle.id;
            }
        }

        public Vec3f Position
        {
            get
            {
                return Light_GetPosition(handle);
            }
            set
            {
                Light_SetPosition(handle, value);
            }
        }

        public Color Color
        {
            get
            {
                return Light_GetColor(handle);
            }
            set
            {
                Light_SetColor(handle, value);
            }
        }

        public float Intensity
        {
            get
            {
                return Light_GetIntensity(handle);
            }
            set
            {
                Light_SetIntensity(handle, value);
            }
        }

        public float Radius
        {
            get
            {
                return Light_GetRadius(handle);
            }
            set
            {
                Light_SetRadius(handle, value);
            }
        }

        public float Falloff
        {
            get
            {
                return Light_GetFalloff(handle);
            }
            set
            {
                Light_SetFalloff(handle, value);
            }
        }

        [DllImport("hyperion", EntryPoint = "Light_GetTypeID")]
        private static extern TypeID Light_GetTypeID();

        [DllImport("hyperion", EntryPoint = "Light_Create")]
        private static extern ManagedHandle Light_Create(
            LightType type,
            Vec3f position,
            Color color,
            float intensity,
            float radius
        );

        [DllImport("hyperion", EntryPoint = "Light_Init")]
        private static extern void Light_Init(ManagedHandle texture);

        [DllImport("hyperion", EntryPoint = "Light_GetPosition")]
        private static extern Vec3f Light_GetPosition(ManagedHandle light);

        [DllImport("hyperion", EntryPoint = "Light_SetPosition")]
        private static extern void Light_SetPosition(ManagedHandle light, Vec3f position);

        [DllImport("hyperion", EntryPoint = "Light_GetColor")]
        private static extern Color Light_GetColor(ManagedHandle light);

        [DllImport("hyperion", EntryPoint = "Light_SetColor")]
        private static extern void Light_SetColor(ManagedHandle light, Color color);

        [DllImport("hyperion", EntryPoint = "Light_GetIntensity")]
        private static extern float Light_GetIntensity(ManagedHandle light);

        [DllImport("hyperion", EntryPoint = "Light_SetIntensity")]
        private static extern void Light_SetIntensity(ManagedHandle light, float intensity);

        [DllImport("hyperion", EntryPoint = "Light_GetRadius")]
        private static extern float Light_GetRadius(ManagedHandle light);

        [DllImport("hyperion", EntryPoint = "Light_SetRadius")]
        private static extern void Light_SetRadius(ManagedHandle light, float radius);

        [DllImport("hyperion", EntryPoint = "Light_GetFalloff")]
        private static extern float Light_GetFalloff(ManagedHandle light);

        [DllImport("hyperion", EntryPoint = "Light_SetFalloff")]
        private static extern void Light_SetFalloff(ManagedHandle light, float falloff);
    }
}