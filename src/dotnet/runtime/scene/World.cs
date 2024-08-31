using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="World")]
    public class World : HypObject
    {
        public World()
        {
        }

        public IDBase ID
        {
            get
            {
                return (IDBase)GetProperty(PropertyNames.ID)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }
    }
}