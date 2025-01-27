using System;
using System.Runtime.InteropServices;
using System.Reflection;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct | AttributeTargets.Enum, Inherited = false)]
    public class HypClassBinding : Attribute
    {
        public string? Name { get; set; }
        public bool IsDynamic { get; set; }

        public HypClass GetClass(Type type)
        {
            HypClass? hypClass;

            if (IsDynamic)
            {
                hypClass = DynamicHypStruct.GetOrCreate(type).HypClass;
            }
            else
            {
                string typeName = Name ?? type.Name;

                hypClass = HypClass.GetClass(typeName);
            }

            if (hypClass == null || !((HypClass)hypClass).IsValid)
            {
                throw new Exception("Failed to load HypClass for type " + type.Name);
            }

            return (HypClass)hypClass;
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