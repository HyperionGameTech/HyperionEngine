using System;
using System.Runtime.InteropServices;
using System.Reflection;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct | AttributeTargets.Enum, Inherited = false)]
    public class ClassBinding : Attribute
    {
        public string? Name { get; set; }
        public bool IsDynamic { get; set; }

        public Class GetClass(Type type)
        {
            /// \todo Needs to deal with DynamicStruct.

            // temp; refactor
            if (type.IsValueType && IsDynamic)
            {
                return DynamicStruct.GetOrCreate(type).Class;
            }

            Class? cls = Class.TryGetClass(type);

            if (cls == null || !((Class)cls).IsValid)
            {
                throw new Exception("Failed to load Class for type " + type.Name);
            }

            return (Class)cls;
        }

        public static ClassBinding? ForType(Type type) 
        {
            Type? currentType = type;

            do
            {
                Attribute? attribute = Attribute.GetCustomAttribute((Type)currentType, typeof(ClassBinding));

                if (attribute != null)
                {
                    return (ClassBinding)attribute;
                }

                currentType = ((Type)currentType).BaseType;
            } while (currentType != null);

            return null;
        }
    }
}