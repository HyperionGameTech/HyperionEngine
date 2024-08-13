using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class PropertyNames
    {
        public static Name ID { get; } = Name.FromString("ID", weak: true);
        public static Name World { get; } = Name.FromString("World", weak: true);
        public static Name GameTime { get; } = Name.FromString("GameTime", weak: true);
    }
}