using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Entity")]
    public class Entity : HypObject
    {
        public Entity()
        {
        }

        public IDBase ID
        {
            get
            {
                return (IDBase)GetProperty(new Name("ID", weak: true))
                    .Get(this)
                    .GetValue();
            }
        }
    }
}