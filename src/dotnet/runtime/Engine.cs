using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Engine")]
    public class Engine : HypObject
    {
        private static Engine instance = new Engine();

        public Engine()
        {
        }

        public static Engine Instance
        {
            get
            {
                return instance;
            }
        }

        public World? World
        {
            get
            {
                return (World?)GetProperty(PropertyNames.World)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }
    }
}