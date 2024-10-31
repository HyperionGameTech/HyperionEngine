using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Struct, Inherited = false)]
    public class NoManagedClass : Attribute
    {
    }

    [NoManagedClass]
    public struct ObjectWrapper
    {
        public object obj;
    }
}