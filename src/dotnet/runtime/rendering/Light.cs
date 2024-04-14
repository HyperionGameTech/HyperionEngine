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
            handle = new ManagedHandle();
            Light_Create(
                type,
                ref position,
                ref color,
                intensity,
                radius,
                out handle
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
                Vec3f lightPosition = new Vec3f();
                Light_GetPosition(handle, out lightPosition);
                return lightPosition;
            }
            set
            {
                Light_SetPosition(handle, ref value);
            }
        }

        public Color Color
        {
            get
            {
                Color color = new Color();
                Light_GetColor(handle, out color);
                return color;
            }
            set
            {
                Light_SetColor(handle, ref value);
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
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID Light_GetTypeID();

        [DllImport("hyperion", EntryPoint = "Light_Create")]
        private static extern void Light_Create(
            [MarshalAs(UnmanagedType.U4)] LightType type,
            [In] ref Vec3f position,
            [In] ref Color color,
            float intensity,
            float radius,
            [Out] out ManagedHandle handle
        );

        [DllImport("hyperion", EntryPoint = "Light_Init")]
        private static extern void Light_Init(ManagedHandle texture);

        [DllImport("hyperion", EntryPoint = "Light_GetPosition")]
        private static extern void Light_GetPosition(ManagedHandle light, [Out] out Vec3f lightPosition);

        [DllImport("hyperion", EntryPoint = "Light_SetPosition")]
        private static extern void Light_SetPosition(ManagedHandle light, [In] ref Vec3f position);

        [DllImport("hyperion", EntryPoint = "Light_GetColor")]
        private static extern void Light_GetColor(ManagedHandle light, [Out] out Color color);

        [DllImport("hyperion", EntryPoint = "Light_SetColor")]
        private static extern void Light_SetColor(ManagedHandle light, [In] ref Color color);

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