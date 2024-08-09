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

        public HypClass LoadHypClass()
        {
            HypClass? hypClass = HypClass.GetClass(Name);

            if (hypClass == null)
            {
                throw new Exception("Failed to load HypClass: " + Name);
            }

            return (HypClass)hypClass;
        }
    }
}
