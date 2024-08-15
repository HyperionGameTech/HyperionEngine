using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class HypStructRegistry
    {
        public static void Initialize()
        {
            Console.WriteLine("Registering HypStructs");

            // Register all structs with the HypStruct attribute
            foreach (Type type in typeof(HypStructRegistry).Assembly.GetTypes())
            {
                foreach (var attribute in type.GetCustomAttributes(typeof(HypStructBinding), false))
                {
                    RegisterStruct(((HypStructBinding)attribute).HypClass, type);
                }
            }
        }

        public static void RegisterStruct(HypClass hypClass, Type type)
        {
            if (!hypClass.IsValid)
            {
                throw new Exception("Failed to register struct; HypClass " + hypClass.Name + " is invalid");
            }

            if (!hypClass.IsStructType)
            {
                throw new Exception("Failed to register struct; HypClass " + hypClass.Name + " is not a struct type");
            }

            if (hypClass.Size != Marshal.SizeOf(type))
            {
                throw new Exception("Failed to register struct; HypClass " + hypClass.Name + " size does not match struct size (HypClass.Size: " + hypClass.Size + ", Struct.Size: " + Marshal.SizeOf(type) + ")");
            }

            Console.WriteLine("Registering struct {0} with HypClass {1}", type.Name, hypClass.Name);

            structs[hypClass.Address] = type;
        }

        public static Type? GetStructType(HypClass hypClass)
        {
            if (!hypClass.IsValid)
            {
                throw new Exception("Failed to get struct type; HypClass " + hypClass.Name + " is invalid");
            }

            if (!hypClass.IsStructType)
            {
                throw new Exception("Failed to get struct type; HypClass " + hypClass.Name + " is not a struct type");
            }

            if (!structs.TryGetValue(hypClass.Address, out Type? type))
            {
                return null;
            }

            return type;
        }

        public static object CreateInstance(HypClass hypClass)
        {
            if (!hypClass.IsValid)
            {
                throw new Exception("Failed to create struct instance; HypClass " + hypClass.Name + " is invalid");
            }

            if (!hypClass.IsStructType)
            {
                throw new Exception("Failed to create struct instance; HypClass " + hypClass.Name + " is not a struct type");
            }

            Type? type = GetStructType(hypClass);

            if (type == null)
            {
                throw new Exception("Failed to create struct instance; HypClass " + hypClass.Name + " is not registered");
            }

            return Activator.CreateInstance(type);
        }

        private static Dictionary<IntPtr, Type> structs = new Dictionary<IntPtr, Type>();
    }

    public class HypStructHelpers
    {
        // public static T MarshalHypStruct<T>(HypClass hypClass)
        // {
        //     if (!hypClass.IsValid)
        //     {
        //         throw new Exception("Failed to marshal HypStruct; HypClass " + hypClass.Name + " is invalid");
        //     }

        //     if (!hypClass.IsStructType)
        //     {
        //         throw new Exception("Failed to marshal HypStruct; HypClass " + hypClass.Name + " is not a struct type");
        //     }

        //     if (hypClass.Size != Marshal.SizeOf<T>())
        //     {
        //         throw new Exception("Failed to marshal HypStruct; HypClass " + hypClass.Name + " size does not match struct size (HypClass.Size: " + hypClass.Size + ", Struct.Size: " + Marshal.SizeOf<T>() + ")");
        //     }

        //     // Allocate memory for the struct
        //     IntPtr structPtr = Marshal.AllocHGlobal(Marshal.SizeOf<T>());
        // }

        public static bool IsHypStruct(Type type, out HypClass? outHypClass)
        {
            outHypClass = null;

            HypClassBinding hypClassBindingAttribute = (HypClassBinding)Attribute.GetCustomAttribute(type, typeof(HypClassBinding));

            if (hypClassBindingAttribute != null)
            {
                HypClass hypClass = hypClassBindingAttribute.HypClass;

                if (hypClass.IsValid && hypClass.IsStructType)
                {
                    outHypClass = hypClass;

                    return true;
                }
            }

            return false;
        }
    }
}