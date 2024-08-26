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

        public float GameTime
        {
            get
            {
                return (float)GetProperty(PropertyNames.GameTime)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }

        public Subsystem? GetSubsystem(TypeID typeId)
        {
            return (Subsystem?)GetMethod(MethodNames.GetSubsystem)
                .Invoke(this, new HypData(typeId.Value))
                .GetValue();
        }
    }
}