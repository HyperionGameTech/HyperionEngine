using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="SubsystemBase")]
    public class Subsystem : HypObject
    {
        protected Subsystem()
        {
        }

        public string Name
        {
            get
            {
                return (string)GetProperty(PropertyNames.Name)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }
    }
}