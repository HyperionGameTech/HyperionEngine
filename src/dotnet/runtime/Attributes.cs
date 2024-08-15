using System;
using System.Reflection;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Method, Inherited = true)]
    public class ScriptMethodStub : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct, Inherited = false)]
    public class HypClassBinding : Attribute
    {
        public string Name { get; set; }

        public HypClass HypClass
        {
            get
            {
                HypClass? hypClass = HypClass.GetClass(Name);

                if (hypClass == null || !((HypClass)hypClass).IsValid)
                {
                    throw new Exception("Failed to load HypClass: " + Name);
                }

                return (HypClass)hypClass;
            }
        }
    }

    [AttributeUsage(AttributeTargets.Struct, Inherited = false)]
    public class HypStructBinding : Attribute
    {
        public string Name { get; set; }

        public HypClass HypClass
        {
            get
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
}
