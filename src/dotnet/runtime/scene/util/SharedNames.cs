using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class PropertyNames
    {
        public static Name ID { get; } = Name.FromString("ID", weak: true);
    }
}