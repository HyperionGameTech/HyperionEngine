using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class HypStructHelpers
    {
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