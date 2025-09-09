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
            // @TODO Needs to deal with DynamicHypStruct.

            // temp; refactor
            if (type.IsValueType && IsDynamic)
            {
                return DynamicHypStruct.GetOrCreate(type).HypClass;
            }

            HypClass? hypClass = HypClass.TryGetClass(type);

            if (hypClass == null || !((HypClass)hypClass).IsValid)
            {
                throw new Exception("Failed to load HypClass for type " + type.Name);
            }

            return (HypClass)hypClass;
        }

        public static HypClassBinding? ForType(Type type) 
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
            } while (currentType != null);

            return null;
        }
    }
}