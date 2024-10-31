using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Method, Inherited = true)]
    public class ScriptMethodStub : Attribute
    {
    }

    /// @TODO: For HypClassBinding, if the target is a struct, we need to validate that all fields match the HypClass 
    /// and we can validate, if the "Size" attribute is there, that they match the size of the struct.

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

        public static HypClassBinding? ForType(Type type, bool inheritance = false) 
        {
            Type? currentType = type;

            do
            {
                Attribute? attribute = Attribute.GetCustomAttribute((Type)currentType, typeof(HypClassBinding));

                if (attribute != null)
                {
                    return (HypClassBinding)attribute;
                }

                currentType = ((Type)currentType).BaseType;
            } while (currentType != null && inheritance);

            return null;
        }
    }
}
