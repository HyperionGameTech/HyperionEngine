using System;
using System.Reflection;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Method, Inherited = true)]
    public class ScriptMethodStub : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.Class, Inherited = true)]
    public class HypClassBinding : Attribute
    {
        public string Name { get; set; }
    }
}
