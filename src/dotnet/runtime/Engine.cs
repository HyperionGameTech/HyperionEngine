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
                return (Engine)HypObject.GetMethod(HypClass.GetClass(typeof(Engine)), new Name("GetInstance", weak: true))
                    .Invoke()
                    .GetValue();
            }
        }

        public Engine()
        {
        }
    }
}