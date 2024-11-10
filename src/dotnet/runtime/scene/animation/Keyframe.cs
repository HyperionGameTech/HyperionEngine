using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Keyframe")]
    [StructLayout(LayoutKind.Sequential, Size=128)]
    public struct Keyframe
    {
        private float time;
        private Transform transform;

        public Keyframe(float time, Transform transform)
        {
            this.time = time;
            this.transform = transform;
        }

        public float Time
        {
            get { return time; }
            set { time = value; }
        }

        public Transform Transform
        {
            get { return transform; }
            set { transform = value; }
        }
    }
}