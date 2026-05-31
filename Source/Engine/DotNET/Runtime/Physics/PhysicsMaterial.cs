using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "PhysicsMaterial")]
    public struct PhysicsMaterial
    {
        public float Mass = 0.0f;

        public PhysicsMaterial()
        {
        }
    }
}