using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Method, Inherited = true)]
    public class ScriptMethodStub : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.Struct, Inherited = false)]
    public class NoManagedClass : Attribute
    {
    }
}
