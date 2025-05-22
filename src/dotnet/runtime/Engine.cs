using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Engine")]
    public class Engine : HypObject
    {
        public static Engine Instance
        {
            get
            {
                using (HypDataBuffer resultData = HypObject.GetMethod(HypClass.GetClass(typeof(Engine)), new Name("GetInstance", weak: true)).InvokeNative())
                {
                    return (Engine)resultData.GetValue();
                }
            }
        }

        public Engine()
        {
        }
    }
}