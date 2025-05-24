using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Engine")]
    public class Engine : HypObject
    {
        private static Engine? instance = null;
        
        public static Engine Instance
        {
            get
            {
                if (instance == null)
                {
                    using (HypDataBuffer resultData = HypObject.GetMethod(HypClass.GetClass(typeof(Engine)), new Name("GetInstance", weak: true)).InvokeNative())
                    {
                        instance = (Engine)resultData.GetValue();
                    }
                }

                return (Engine)instance;
            }
        }

        public Engine()
        {
        }
    }
}