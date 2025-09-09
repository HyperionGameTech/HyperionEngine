using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Color")]
    [StructLayout(LayoutKind.Explicit, Size = 4)]
    public struct Color
    {
        [FieldOffset(0)]
        private uint value;

        public Color()
        {
            this.value = 0;
        }

        public Color(uint value)
        {
            this.value = value;
        }

        public Color(float r, float g, float b, float a)
        {
            this.value = (uint)(a * 255.0f) << 24 | (uint)(b * 255.0f) << 16 | (uint)(g * 255.0f) << 8 | (uint)(r * 255.0f);
        }

        public float Red
        {
            get
            {
                return (float)((value & 0x000000FF) >> 0) / 255.0f;
            }
            set
            {
                value = Math.Clamp(value, 0.0f, 1.0f);
                value = value * 255.0f;
                uint uvalue = (uint)value;
                uvalue = uvalue << 0;
                this.value = (this.value & 0xFFFFFF00) | uvalue;
            }
        }

        public float Green
        {
            get
            {
                return (float)((value & 0x0000FF00) >> 8) / 255.0f;
            }
            set
            {
                value = Math.Clamp(value, 0.0f, 1.0f);
                value = value * 255.0f;
                uint uvalue = (uint)value;
                uvalue = uvalue << 8;
                this.value = (this.value & 0xFFFF00FF) | uvalue;
            }
        }

        public float Blue
        {
            get
            {
                return (float)((value & 0x00FF0000) >> 16) / 255.0f;
            }
            set
            {
                value = Math.Clamp(value, 0.0f, 1.0f);
                value = value * 255.0f;
                uint uvalue = (uint)value;
                uvalue = uvalue << 16;
                this.value = (this.value & 0xFF00FFFF) | uvalue;
            }
        }

        public float Alpha
        {
            get
            {
                return (float)((value & 0xFF000000) >> 24) / 255.0f;
            }
            set
            {
                value = Math.Clamp(value, 0.0f, 1.0f);
                value = value * 255.0f;
                uint uvalue = (uint)value;
                uvalue = uvalue << 24;
                this.value = (this.value & 0x00FFFFFF) | uvalue;
            }
        }
    }
}